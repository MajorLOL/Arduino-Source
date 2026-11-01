/*  Wild Encounter Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_WildEncounterReader_H
#define PokemonAutomation_PokemonFRLG_WildEncounterReader_H

#include <optional>
#include <string>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"


namespace PokemonAutomation{

class Logger;
class ImageViewRGB32;
class VideoOverlaySet;
namespace Pokemon{
    class PokemonNameReader;
}

namespace NintendoSwitch{
namespace PokemonFRLG{

struct PokemonFRLG_WildEncounter{
    std::string name;
    std::optional<unsigned> level;
    // gender
};

class WildEncounterReader {
public:
    static constexpr double MAX_LOG10P = -1.40;

    WildEncounterReader(Color color = COLOR_RED);

    void make_overlays(VideoOverlaySet& items) const;

    // Reads species from the top left of the battle screen
    // by matching to the provided subset of species 
    PokemonFRLG_WildEncounter read_encounter(
        Logger& logger, Language language,
        const ImageViewRGB32& frame, 
        const std::set<std::string>& subset,
        double max_log10p = MAX_LOG10P
    );

    // Same, with a name reader made by the caller. Making one for a subset
    // loads the name lists of every language: a caller that reads often
    // can make it once.
    PokemonFRLG_WildEncounter read_encounter_with_reader(
        Logger& logger, Language language,
        const ImageViewRGB32& frame,
        const Pokemon::PokemonNameReader& name_reader,
        double max_log10p = MAX_LOG10P
    );

private:
    Color m_color;
    ImageFloatBox m_box_name;
    ImageFloatBox m_box_name_jpn;
    // ImageFloatBox m_box_level;
};


void add_tests_WildEncounterReader(UnitTestDatabase& database);

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif


/*  PokemonLZA - Mission complete detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detector for mission-complete cards.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_MissionCompleteDetector_H
#define PokemonAutomation_PokemonLZA_MissionCompleteDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects the "COMPLETE" card shown when a main mission finishes (waits for A).
class MissionCompleteDetector : public StaticScreenDetector{
public:
    explicit MissionCompleteDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_word;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
    ImageFloatBox m_bar_left;
    ImageFloatBox m_bar_right;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

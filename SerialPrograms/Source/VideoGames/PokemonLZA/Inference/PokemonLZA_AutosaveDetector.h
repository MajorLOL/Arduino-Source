/*  PokemonLZA - Autosave detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Language-independent detector for the top-right autosave badge.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutosaveDetector_H
#define PokemonAutomation_PokemonLZA_AutosaveDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects the top-right autosave pencil icon in any game language.
class AutosaveDetector : public StaticScreenDetector{
public:
    explicit AutosaveDetector(Color color = COLOR_YELLOW);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_left;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

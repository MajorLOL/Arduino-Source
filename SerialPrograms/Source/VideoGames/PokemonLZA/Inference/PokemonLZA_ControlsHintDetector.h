/*  PokemonLZA - Controls hint detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Language-independent detector for the tutorial controls hint card.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_ControlsHintDetector_H
#define PokemonAutomation_PokemonLZA_ControlsHintDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects the tutorial controls-hint "?" card shown during free control.
class ControlsHintDetector : public StaticScreenDetector{
public:
    explicit ControlsHintDetector(Color color = COLOR_BLUE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    bool card_at(const ImageViewRGB32& screen, double dy) const;

    Color m_color;
    ImageFloatBox m_card;
    ImageFloatBox m_glyph;
    ImageFloatBox m_around[4];
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

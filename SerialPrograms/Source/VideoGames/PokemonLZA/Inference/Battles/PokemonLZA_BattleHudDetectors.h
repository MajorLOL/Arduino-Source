/*  PokemonLZA - Battle HUD detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detectors for the battle lead panel and battle-only status markers.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_BattleHudDetectors_H
#define PokemonAutomation_PokemonLZA_BattleHudDetectors_H

#include <vector>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects the player's lead Pokemon panel at the bottom left.
//  Also appears in wild zones; pair with BattleMarkersDetector for battle-only detection.
class BattlePanelDetector : public StaticScreenDetector{
public:
    explicit BattlePanelDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_badge;
    ImageFloatBox m_bar;
};

//  Detects the battle-only row of five status markers beside the lead panel.
class BattleMarkersDetector : public StaticScreenDetector{
public:
    explicit BattleMarkersDetector(Color color = COLOR_BLUE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    std::vector<ImageFloatBox> m_slots;
};

//  Detects the opponent trainer's top-center Pokeball strip during trainer battles.
//  Shown from frame 1 of battle even before move hints or stat-stage markers appear.
class TrainerPokeballsDetector : public StaticScreenDetector{
public:
    explicit TrainerPokeballsDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    struct BallSlot{
        ImageFloatBox top;
        ImageFloatBox bottom;
    };

    Color m_color;
    std::vector<BallSlot> m_slots;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

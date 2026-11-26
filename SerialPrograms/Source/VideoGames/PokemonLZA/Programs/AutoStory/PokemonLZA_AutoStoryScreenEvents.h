/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's detectors exposed as shared ScreenEvents and start-screen signatures.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_BattleHudDetectors.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryDialogDetector.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Maps PLZA screen detectors to shared ScreenEvents.
class PlzaScreenObserver : public ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;

    //  Runs detection directly on an image view without a VideoSnapshot wrapper.
    ScreenEvents read(const ImageViewRGB32& screen);

    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    StoryDialogDetector m_dialog;
    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
    ButtonDetector m_battle_hint;
    BattlePanelDetector m_battle_panel;
    BattleMarkersDetector m_battle_markers;
    TrainerPokeballsDetector m_trainer_pokeballs;
    PartyBarDetector m_party;
};

//  Reads overworld start-signature fields (talk_prompt, party, minimap) from `screen`.
//  Returns an empty signature on fading or dialogue frames; wild_zone requires the full map.
StartSignature read_start_screen(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

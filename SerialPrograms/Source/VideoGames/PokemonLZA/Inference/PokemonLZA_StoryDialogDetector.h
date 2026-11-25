/*  PokemonLZA - Story dialog detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detectors for story dialogue boxes and the "press A to continue" chevron.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_StoryDialogDetector_H
#define PokemonAutomation_PokemonLZA_StoryDialogDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Whether white dialogue detection requires the advance arrow or only the box.
enum class WhiteDialogPart{
    BOX_AND_ARROW,
    BOX_ONLY,
};

//  Configuration selecting which white dialogue variant StoryDialogDetector accepts.
struct StoryDialogKinds{
    WhiteDialogPart white = WhiteDialogPart::BOX_AND_ARROW;
};

//  Detects any story dialogue box: white, blue, or teal.
class StoryDialogDetector : public StaticScreenDetector{
public:
    explicit StoryDialogDetector(StoryDialogKinds kinds = {});

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    StoryDialogKinds m_kinds;
    FlatWhiteDialogDetector m_white;
    BlueDialogDetector m_blue;
    TealDialogDetector m_teal;
};

//  Detects the advance chevron at the bottom right of dialogue boxes or boxless subtitles.
class AdvanceArrowDetector : public StaticScreenDetector{
public:
    explicit AdvanceArrowDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_area;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Party Bar Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_PartyBarDetector_H
#define PokemonAutomation_PokemonLZA_PartyBarDetector_H

#include <array>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"
#include "PokemonLZA_OverworldPartySelectionDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Counts the filled slots of the overworld party bar (bottom left, 6 slots). The bar
//  shows when its D-pad marker does (party of 2+) or its last slot shows empty (party
//  of 1-5). A filled slot has an HP bar, or a Poke Ball if its partner is sent out.
class PartyBarDetector : public StaticScreenDetector{
public:
    PartyBarDetector(Color color = COLOR_RED, VideoOverlay* overlay = nullptr);

    virtual void make_overlays(VideoOverlaySet& items) const override;

    //  Whether the party bar is on screen; then filled_slots() is its count.
    virtual bool detect(const ImageViewRGB32& screen) override;

    virtual void reset_state() override{
        m_selection.reset_state();
        m_filled_slots = 0;
    }

    //  0 when the last detect() saw no bar.
    int filled_slots() const{ return m_filled_slots; }

private:
    //  Mostly HP color, or darker than the card above it (the trough).
    bool slot_has_hp_bar(const ImageViewRGB32& screen, size_t index) const;

    OverworldPartySelectionDetector m_selection;
    //  The last slot's frame, read when it is empty.
    ImageFloatBox m_last_slot;
    //  Each slot's HP bar and, above it, the card between icon and bar.
    std::array<ImageFloatBox, 6> m_bar_boxes;
    std::array<ImageFloatBox, 6> m_card_boxes;
    int m_filled_slots = 0;
};


//  Registers the party bar detector's tests on the PartyBarDetector frames.
void add_tests_PartyBarDetector(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

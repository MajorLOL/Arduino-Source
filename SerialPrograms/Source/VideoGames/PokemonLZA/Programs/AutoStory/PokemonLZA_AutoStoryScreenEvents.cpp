/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_MinimapMatch.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  run_towards_gate_with_A_button()'s box cut 0.05 shorter at the bottom,
//  where it would reach the battle HUD's hints (BATTLE_HINT_BOX).
const ImageFloatBox INTERACT_PROMPT_BOX{0.3, 0.2, 0.4, 0.65};
//  The A of "X A: Select Move" at the bottom centre (read on 15 of 15
//  battle frames, none of 442 others).
const ImageFloatBox BATTLE_HINT_BOX{0.40, 0.84, 0.20, 0.10};

} // namespace


PlzaScreenObserver::PlzaScreenObserver()
    : m_dialog({.white = WhiteDialogPart::BOX_ONLY})
    , m_choice_arrow(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box())
    , m_interact(COLOR_RED, ButtonType::ButtonA, INTERACT_PROMPT_BOX)
    , m_battle_hint(COLOR_CYAN, ButtonType::ButtonA, BATTLE_HINT_BOX)
    , m_battle_panel(COLOR_GREEN)
    , m_battle_markers(COLOR_BLUE)
    , m_trainer_pokeballs(COLOR_MAGENTA)
    , m_party(COLOR_WHITE)
{}

ScreenEvents PlzaScreenObserver::observe(const VideoSnapshot& frame, WallClock){
    if (!frame){
        return {};
    }
    return read(*frame.frame);
}

ScreenEvents PlzaScreenObserver::read(const ImageViewRGB32& screen){
    ScreenEvents out = fade_events(fade_of_screen(screen));
    if (fade_in(out) != WarpFade::NONE){
        return out;
    }
    const bool box = m_dialog.detect(screen);
    out.set(ScreenEvent::DIALOG_BOX, box);
    //  The Pokemon Center nurse's menu has no selection arrow, only its
    //  highlighted row (console B 2026-11-19, Traditional Chinese).
    out.set(ScreenEvent::CHOICE_BOX, m_choice_arrow.detect(screen) || (box && read_dialog_choice_rows(screen).has_value()));
    out.set(ScreenEvent::INTERACT_PROMPT, !box && m_interact.detect(screen));
    const bool battle = m_battle_hint.detect(screen) ||
        (m_battle_panel.detect(screen) && (m_battle_markers.detect(screen) || m_trainer_pokeballs.detect(screen)));
    out.set(ScreenEvent::BATTLE_STARTED, battle);
    out.set(ScreenEvent::PLAYER_CAN_MOVE, !box && m_party.detect(screen));
    return out;
}

void PlzaScreenObserver::make_overlays(VideoOverlaySet& items) const{
    m_dialog.make_overlays(items);
    m_choice_arrow.make_overlays(items);
    m_interact.make_overlays(items);
    m_battle_hint.make_overlays(items);
    m_battle_panel.make_overlays(items);
    m_battle_markers.make_overlays(items);
    m_trainer_pokeballs.make_overlays(items);
    m_party.make_overlays(items);
}

StartSignature read_start_screen(const ImageViewRGB32& screen){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(screen);
    const bool unreadable = events.has(ScreenEvent::FADE_BLACK) || events.has(ScreenEvent::FADE_WHITE)
        || events.has(ScreenEvent::DIALOG_BOX);
    if (unreadable){
        return StartSignature{};
    }
    PartyBarDetector party_bar;
    std::optional<size_t> party;
    if (party_bar.detect(screen) && party_bar.filled_slots() > 0){
        party = (size_t)party_bar.filled_slots();
    }
    return StartSignature{
        .talk_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .party = party,
        .minimap = matching_minimap(screen),
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

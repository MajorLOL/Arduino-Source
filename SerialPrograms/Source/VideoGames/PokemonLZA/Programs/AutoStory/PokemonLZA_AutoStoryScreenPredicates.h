/*  PokemonLZA AutoStory - Screen predicates
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared declarative ScreenMatch predicates over one ScreenObservation for
 *  PLZA AutoStory actions and the scene machine.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenPredicates_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenPredicates_H

#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenMatch.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A black or white fade.
inline const Pokemon::ScreenMatch FADE_SHOWS{
    .any_events = {ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE},
};

//  A text box, or the advance chevron without one.
inline const Pokemon::ScreenMatch TEXT_SHOWS{
    .any_events = {ScreenEvent::DIALOG_BOX},
    .any_readings = {ADVANCE_ARROW_READING},
};

//  A text box with the advance chevron waiting for A.
inline const Pokemon::ScreenMatch TEXT_WAITING_FOR_A{
    .all_events = {ScreenEvent::DIALOG_BOX},
    .all_readings = {ADVANCE_ARROW_READING},
};

//  The party bar with no text box over it.
inline const Pokemon::ScreenMatch PARTY_BAR_FREE{
    .all_events = {ScreenEvent::PLAYER_CAN_MOVE},
    .no_events = {ScreenEvent::DIALOG_BOX},
};

//  The party bar, the controls hint, or an A prompt, none under a text box.
inline const Pokemon::ScreenMatch CONTROL_SHOWS{
    .any_events = {ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::INTERACT_PROMPT},
    .no_events = {ScreenEvent::DIALOG_BOX},
    .any_readings = {CONTROLS_HINT_READING},
};

//  The party bar or the controls hint with no dialog, fade, or advance chevron.
inline const Pokemon::ScreenMatch OVERWORLD_FREE{
    .any_events = {ScreenEvent::PLAYER_CAN_MOVE},
    .no_events = {ScreenEvent::DIALOG_BOX, ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE},
    .any_readings = {CONTROLS_HINT_READING},
    .no_readings = {ADVANCE_ARROW_READING},
};

//  The controls hint on the platform, uncovered by autosave or a text box.
inline const Pokemon::ScreenMatch PLATFORM_HINT{
    .no_events = {ScreenEvent::DIALOG_BOX},
    .all_readings = {CONTROLS_HINT_READING},
    .no_readings = {AUTOSAVE_READING},
};

//  A tutorial card or a mission-complete card.
inline const Pokemon::ScreenMatch CARD_SHOWS{
    .any_readings = {TUTORIAL_CARD_READING, MISSION_COMPLETE_READING},
};

//  Text, a fade, or a card: the screen is busy.
inline const Pokemon::ScreenMatch SCENE_BUSY{
    .any_events = {ScreenEvent::DIALOG_BOX, ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE},
    .any_readings = {ADVANCE_ARROW_READING, MISSION_COMPLETE_READING, TUTORIAL_CARD_READING},
};

//  A fade, text box, or choice box: an A press at a prompt was taken.
inline const Pokemon::ScreenMatch PROMPT_TAKEN{
    .any_events = {ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE, ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX},
};

//  A choice box whose highlighted row and row count both read.
inline const Pokemon::ScreenMatch CHOICE_WITH_ROWS{
    .all_events = {ScreenEvent::CHOICE_BOX},
    .all_readings = {CHOICE_SELECTED_READING, CHOICE_ROWS_READING},
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

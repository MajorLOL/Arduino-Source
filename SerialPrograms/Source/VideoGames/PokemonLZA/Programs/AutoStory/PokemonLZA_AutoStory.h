/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays Pokemon Legends: Z-A's main story on the shared story engine.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "Pokemon/AutoStory/Options/Pokemon_StoryRangeOptions.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Descriptor for the Pokemon Legends: Z-A AutoStory program.
class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
};


//  Runs the story's checkpoints from the start one to the end one.
enum class StarterChoice{
    CHIKORITA,
    TOTODILE,
    TEPIG,
};

class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    Pokemon::StoryRangeOptions RANGE;
    EnumDropdownOption<NewGameLanguage> GAME_LANGUAGE;
    SimpleIntegerOption<uint8_t> PLAYER_LOOK;
    StringOption PLAYER_NAME;
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption LOAD_SAVE_FIRST;
    EnumDropdownOption<StarterChoice> STARTER;
    BattleAIOption BATTLE_AI;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationsOption NOTIFICATIONS;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Pokemon FRLG Autostory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Play the FireRed/LeafGreen story from the bedroom to the Hall of Fame,
 *  segment by segment. The routes and steps are generated from the
 *  pret/pokefirered map data and scripts by the scripts in
 *  SerialPrograms/Scripts/PokemonFRLG/AutoStory/ (see the README there).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_H
#define PokemonAutomation_PokemonFRLG_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/ButtonOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "PokemonFRLG_AutoStoryCollection.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

struct StoryScript;
struct StorySegment;
struct StoryRuntime;


class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class AutoStory : public SingleSwitchProgramInstance, private ConfigOption::Listener{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();
    ~AutoStory();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

    enum class Starter{
        BULBASAUR,
        SQUIRTLE,
        CHARMANDER,
    };

    enum class WildBattles{
        FIGHT,
        FLEE,
    };

    enum class GameVersion{
        FIRERED,
        LEAFGREEN,
    };

private:
    //  The game or the starter changed: the POKeDEX table follows.
    virtual void on_config_value_changed(void* object) override;

    //  Run the steps of a segment from "first_step". Saves in the segment
    //  set "rt.resume_at" (where to continue after a reset).
    void run_segment(
        SingleSwitchProgramEnvironment& env, StoryRuntime& rt,
        const StoryScript& script, const StorySegment& segment,
        size_t first_step
    );

    StaticTextOption SETUP_TEXT;
    DeferredStopButtonOption STOP_AFTER_CURRENT;
    EnumDropdownOption<Starter> STARTER;
    EnumDropdownOption<GameVersion> GAME_VERSION;
    BooleanCheckBoxOption NEW_GAME;
    StringOption PLAYER_NAME;
    StringOption RIVAL_NAME;
    BooleanCheckBoxOption CONTINUE_FROM_SAVE;
    IntegerEnumDropdownOption START_SEGMENT;
    SimpleIntegerOption<uint16_t> START_STEP;
    IntegerEnumDropdownOption END_SEGMENT;
    EnumDropdownOption<WildBattles> WILD_BATTLES;
    BooleanCheckBoxOption CATCH_SHINIES;
    SimpleIntegerOption<uint16_t> EXTRA_LEVELS;
    BooleanCheckBoxOption RESET_AT_START;
    BooleanCheckBoxOption SAVE_AFTER_HEAL;
    SimpleIntegerOption<uint16_t> RETRIES;
    SimpleIntegerOption<uint16_t> GRASS_STEP_CHECK_MS;
    BooleanCheckBoxOption VERIFY_WALKING;
    BooleanCheckBoxOption RUNNING;
    StringOption DEX_CAUGHT;
    BooleanCheckBoxOption COLLECTION_SHINY;
    SimpleIntegerOption<uint16_t> COLLECTION_SHINY_MINUTES;
    StaticTextOption COLLECTION_SUMMARY;
    StoryCollectionTable COLLECTION;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

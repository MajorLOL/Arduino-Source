/*  Pokemon FRLG Autostory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Play the FireRed/LeafGreen story from the bedroom to the Hall of Fame,
 *  segment by segment. The routes and steps are generated from the
 *  pret/pokefirered map data and scripts (tools/route_generator).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStory_H
#define PokemonAutomation_PokemonFRLG_AutoStory_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/ButtonOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"

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


class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();
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

private:
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
    IntegerEnumDropdownOption START_SEGMENT;
    SimpleIntegerOption<uint16_t> START_STEP;
    IntegerEnumDropdownOption END_SEGMENT;
    EnumDropdownOption<WildBattles> WILD_BATTLES;
    SimpleIntegerOption<uint16_t> EXTRA_LEVELS;
    BooleanCheckBoxOption RESET_AT_START;
    BooleanCheckBoxOption SAVE_AFTER_HEAL;
    SimpleIntegerOption<uint16_t> RETRIES;
    SimpleIntegerOption<uint16_t> GRASS_STEP_CHECK_MS;
    BooleanCheckBoxOption VERIFY_WALKING;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

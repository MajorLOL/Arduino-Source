/*  Pokemon FRLG Claude Plays
 *
 *  PocketPilot's own program (experimental: not tested on a console yet,
 *  and not against the real Claude API).
 *
 *  Claude, the AI model of Anthropic, plays toward a goal the user writes
 *  down, on the user's own API key (the one of the panel "Claude").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_ClaudePlays_H
#define PokemonAutomation_PokemonFRLG_ClaudePlays_H

#include <string>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/TextEditOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "PocketPilot/Assistant/Assistant_Play.h"
#include "PocketPilot/ClaudePlays/ClaudePlays_Engine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What Claude gets in this program: its tools (with the session's own two,
//  "note" and "done") and its instructions. "allow_save": the user lets
//  Claude save the game. (Here for the test of the interface as well, which
//  checks that they are what the API takes.)
std::vector<PocketPilot::Assistant::PlayTool> claude_plays_tools(bool allow_save);
std::string claude_plays_instructions(bool allow_save);


class ClaudePlays_Descriptor : public SingleSwitchProgramDescriptor{
public:
    ClaudePlays_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class ClaudePlays : public SingleSwitchProgramInstance{
public:
    using Descriptor = ClaudePlays_Descriptor;
    ClaudePlays();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

    using Model = PocketPilot::ClaudePlays::Model;

    //  How large the pictures are that go to Claude.
    enum class Picture{
        SMALL,
        NORMAL,
        LARGE,
    };

private:
    StaticTextOption SETUP_TEXT;
    TextEditOption GOAL;
    EnumDropdownOption<Model> MODEL;
    SimpleIntegerOption<uint32_t> MAX_ACTIONS;
    FloatingPointOption MAX_COST;
    EnumDropdownOption<Picture> PICTURE;
    BooleanCheckBoxOption ALLOW_SAVE;
    BooleanCheckBoxOption OVERLAY_NOTES;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

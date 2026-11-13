/*  Pokemon FRLG Box Namer
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  Gives the PC's boxes the names of a list, one after the other: what an
 *  ACE code asks for, where the names of the boxes are the code and one
 *  wrong character is a wrong code. Every character is read back from the
 *  screen, in the game's own font, before the next one is typed and before
 *  OK is pressed.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BoxNamer_H
#define PokemonAutomation_PokemonFRLG_BoxNamer_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/TextEditOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class BoxNamer_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BoxNamer_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class BoxNamer : public SingleSwitchProgramInstance{
public:
    using Descriptor = BoxNamer_Descriptor;
    BoxNamer();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    StaticTextOption SETUP_TEXT;
    TextEditOption NAMES;
    BooleanCheckBoxOption CHECK_TITLE;
    BooleanCheckBoxOption CHECK_ONLY;

    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

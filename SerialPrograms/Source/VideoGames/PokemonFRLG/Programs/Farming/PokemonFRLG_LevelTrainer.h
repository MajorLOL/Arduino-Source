/*  Pokemon FRLG Level Trainer
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  Trains the first Pokemon of the party to a level, with the training of
 *  the AutoStory: its battles, its grass lines, and at the story's training
 *  places also its routes between the nurse and the grass.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_LevelTrainer_H
#define PokemonAutomation_PokemonFRLG_LevelTrainer_H

#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class LevelTrainer_Descriptor : public SingleSwitchProgramDescriptor{
public:
    LevelTrainer_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class LevelTrainer : public SingleSwitchProgramInstance{
public:
    using Descriptor = LevelTrainer_Descriptor;
    LevelTrainer();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

    //  Where to train: where the player stands, or one of the training
    //  places of the story (from its nurse).
    enum class Place{
        HERE,
        VIRIDIAN_ROUTE2,
        PEWTER_ROUTE2,
        CERULEAN_ROUTE4,
        CERULEAN_ROUTE24,
        VERMILION_ROUTE11,
        CELADON_ROUTE7,
        FUCHSIA_ROUTE15,
        CINNABAR_MANSION,
        INDIGO_VICTORY_ROAD,
    };

    enum class Line{
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };

private:
    StaticTextOption SETUP_TEXT;
    EnumDropdownOption<Place> PLACE;
    SimpleIntegerOption<uint16_t> LEVEL;
    EnumDropdownOption<Line> LINE;
    SimpleIntegerOption<uint16_t> LINE_LENGTH;
    SimpleIntegerOption<uint16_t> STOP_HP;
    SimpleIntegerOption<uint32_t> MAX_BATTLES;
    BooleanCheckBoxOption CATCH_SHINIES;
    BooleanCheckBoxOption SAVE;
    BooleanCheckBoxOption FAST_BATTLES;
    BooleanCheckBoxOption SHORT_GRASS_LOOKS;
    BooleanCheckBoxOption RUNNING;
    BooleanCheckBoxOption OVERLAY_NOTES;
    SimpleIntegerOption<uint16_t> GRASS_STEP_CHECK_MS;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

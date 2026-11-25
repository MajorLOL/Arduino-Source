/*  PocketPilot Claude Plays: the program
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  "Claude Plays" as it stands in the list of every game, and in the list
 *  "Other Games" for whatever else is on the screen (experimental: not
 *  tested on a console yet, and not against the real Claude API).
 *
 *  The user writes down a goal. Claude gets it with the tools of
 *  ClaudePlays_AnyGame.h (look, read_text, press, hold, mash, wait), what
 *  ClaudePlays_Games.h knows about the game, and, where the game's other
 *  programs have one and the user allows it, their routine to save the game.
 *  The session itself is ClaudePlays_Engine.h.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ClaudePlays_Program_H
#define PokemonAutomation_PocketPilot_ClaudePlays_Program_H

#include <memory>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/TextEditOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/Panels/PanelList.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "ClaudePlays_Engine.h"
#include "ClaudePlays_Games.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{


//  The program "Claude Plays" for the list of a game.
std::unique_ptr<PanelDescriptor> make_claude_plays(GameId game);


class GameProgram_Descriptor : public NintendoSwitch::SingleSwitchProgramDescriptor{
public:
    explicit GameProgram_Descriptor(GameId game);

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
    virtual std::unique_ptr<GameConsole::ConsoleProgramInstance> make_instance(
        GameConsole::ConsoleSystemSession& system
    ) const override;

private:
    const GameId m_game;
};


class GameProgram : public NintendoSwitch::SingleSwitchProgramInstance{
public:
    explicit GameProgram(GameId game);

    virtual void program(
        NintendoSwitch::SingleSwitchProgramEnvironment& env, CancellableScope& scope
    ) override;
    //  Whatever is on the screen is played, borders and all.
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

    //  How large the pictures are that go to Claude.
    enum class Picture{
        SMALL,
        NORMAL,
        LARGE,
    };

private:
    const GameId m_game;

    StaticTextOption SETUP_TEXT;
    TextEditOption GOAL;
    TextEditOption ABOUT;
    EnumDropdownOption<Model> MODEL;
    SimpleIntegerOption<uint32_t> MAX_ACTIONS;
    FloatingPointOption MAX_COST;
    EnumDropdownOption<Picture> PICTURE;
    BooleanCheckBoxOption ALLOW_SAVE;
    BooleanCheckBoxOption ALLOW_HOME;
    BooleanCheckBoxOption OVERLAY_NOTES;

    EventNotificationsOption NOTIFICATIONS;
};


//  The list of a game that has no program of its own yet (see
//  has_only_claude_plays()): a section with Claude Plays and nothing else,
//  under a card that the program draws (ClaudePlays_GameCard.h). A game that
//  gets programs gets a list of its own in VideoGames/ and the Claude Plays
//  stays on top of it, as in every other list.
class GameOnlyList : public PanelListDescriptor{
public:
    explicit GameOnlyList(GameId game);

protected:
    virtual std::vector<PanelEntry> make_panels() const override;

private:
    const GameId m_game;
};


//  The list of games "Other Games": Claude Plays for a game that has no
//  list of its own.
class OtherGamesList : public PanelListDescriptor{
public:
    OtherGamesList();

protected:
    virtual std::vector<PanelEntry> make_panels() const override;
};



}
}
}
#endif

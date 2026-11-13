/*  Nugget Bridge Farmer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_StartMenuNavigation.h"
#include "PokemonFRLG_NuggetBridgeFarmer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

NuggetBridgeFarmer_Descriptor::NuggetBridgeFarmer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:NuggetBridgeFarmer",
        Pokemon::STRING_POKEMON + " FRLG", "Nugget Bridge Farmer",
        "Programs/PokemonFRLG/NuggetBridgeFarmer.html",
        "Farm the Nugget Bridge for money.",
        PanelQualityStanding::OFFICIAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct NuggetBridgeFarmer_Descriptor::Stats : public StatsTracker {
    public:
    Stats()
        : nuggets(m_stats["Nuggets"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Nuggets");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& nuggets;
    std::atomic<uint64_t>& errors;
};

std::unique_ptr<StatsTracker> NuggetBridgeFarmer_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

NuggetBridgeFarmer::NuggetBridgeFarmer()
    : STOP_AFTER_CURRENT("Nugget")
    , NUM_NUGGETS(
        "<b>Number of Nuggets:</b><br>"
        "Zero will run until 'Stop after Current Nugget' is pressed or the program is manually stopped.",
        LockMode::UNLOCK_WHILE_RUNNING, 
        100, // About 2 hours of farming. 
        0
    )
    , GO_HOME_WHEN_DONE(false)
    , PERIODIC_SAVE(
        "<b>Periodically Save:</b><br>"
        "Save the game every this many nuggets. This reduces the loss to game crashes. Set to zero to disable.",
        LockMode::UNLOCK_WHILE_RUNNING,
        50,
        0
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(STOP_AFTER_CURRENT);

    PA_ADD_OPTION(NUM_NUGGETS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(PERIODIC_SAVE);

    PA_ADD_OPTION(NOTIFICATIONS);
}

void NuggetBridgeFarmer::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){

    home_black_border_check(env.console, context);

    NuggetBridgeFarmer_Descriptor::Stats& stats = env.current_stats<NuggetBridgeFarmer_Descriptor::Stats>();
    DeferredStopButtonOption::ResetOnExit reset_on_exit(STOP_AFTER_CURRENT);

    //  PocketPilot: every wait below was a loop without an end. The walk to
    //  the Rocket goes by timings alone. Its first leg may come out a tile
    //  short or long, and four others end at a wall or at the Rocket's own
    //  trigger: there some time more or less does not matter. The fifth
    //  (right, along the street in the north of the city) has to come out
    //  at exactly 15 tiles, on the middle one of the bridge's three
    //  columns. One tile less or more and the way up stops at a trainer of
    //  the bridge (they stand in the two outer columns), no battle starts,
    //  and the program mashed B until someone looked. The same with a bag
    //  that has no room for the Nugget. Now a wait that takes far longer
    //  than it can stops the program with an error (and its notification),
    //  and counts in "Errors".
    auto give_up_after = [&](WallClock started, std::chrono::seconds limit, const std::string& message){
        if (current_time() - started <= limit){
            return;
        }
        stats.errors++;
        env.update_stats();
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "NuggetBridgeFarmer: " + message,
            env.console
        );
    };

    for (uint32_t nuggets_since_last_save = 0;; nuggets_since_last_save++){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE);
        if (NUM_NUGGETS != 0 && stats.nuggets >= NUM_NUGGETS){
            break;
        }

        if (PERIODIC_SAVE != 0 && nuggets_since_last_save >= PERIODIC_SAVE){
            env.console.log("Saving game...");

            save_game_to_overworld(env.console, context);
            nuggets_since_last_save = 0;
        }

        env.console.log("Exiting Pokemon Center...");
        WallClock wait_start = current_time();
        while (true){
            BlackScreenWatcher pokemon_ceter_exit(COLOR_RED);

            int ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_press_dpad(context, DPAD_DOWN, 2000ms, 0ms);
                },
                { pokemon_ceter_exit }
            );

            if (ret == 0){
                break;
            }
            give_up_after(
                wait_start, std::chrono::seconds(30),
                "Did not leave the Pokemon Center within 30 seconds of walking down. Start in front of the "
                "nurse of Cerulean City's Pokemon Center. (After a battle that was won instead of lost the "
                "player is still on the bridge: the Rocket gives a Nugget each time only until he is beaten.)"
            );
        }

        env.console.log("Detecting overworld...");
        wait_start = current_time();
        while (true){
            BlackScreenOverWatcher overworld_entered(COLOR_RED);

            int ret = wait_until(
                env.console, context,
                std::chrono::milliseconds(2000),
                {overworld_entered}
            );

            if (ret == 0){
                break;
            }
            give_up_after(
                wait_start, std::chrono::seconds(30),
                "The city did not show within 30 seconds of leaving the Pokemon Center."
            );
        }

        // There is a small delay from seeing the overworld to being able to a actually move.
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();

        env.console.log("Navigating to Team Rocket member...");

        ssf_press_button(context, BUTTON_B, 0ms, 12000ms);
        pbf_press_dpad(context, DPAD_LEFT, 590ms, 0ms);
        pbf_press_dpad(context, DPAD_UP, 510ms, 0ms);
        pbf_press_dpad(context, DPAD_LEFT, 1550ms, 0ms);
        pbf_press_dpad(context, DPAD_UP, 1175ms, 0ms);
        pbf_press_dpad(context, DPAD_RIGHT, 1950ms, 0ms);
        pbf_press_dpad(context, DPAD_UP, 5300ms, 0ms);

        context.wait_for_all_requests();

        env.console.log("Starting battle...");
        wait_start = current_time();
        while (true){
            BattleMenuWatcher battle_menu(COLOR_RED);

            int ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 2000ms);
                },
                { battle_menu }
            );

            if (ret == 0){
                break;
            }
            //  His talk, the Nugget's jingle and the start of the battle
            //  take about 20 seconds.
            give_up_after(
                wait_start, std::chrono::seconds(90),
                "The battle with the Rocket did not start within 90 seconds of the walk to him. The walk "
                "goes by timings alone and did not end in front of him, or he has no Nugget to give: the "
                "bag has no room for it, or he was beaten before."
            );
        }

        env.console.log("Loosing battle...");
        wait_start = current_time();
        while (true){
            BlackScreenWatcher battle_lost(COLOR_RED);

            int ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_A, 2000ms);
                },
                { battle_lost }
            );

            if (ret == 0){
                break;
            }
            give_up_after(
                wait_start, std::chrono::seconds(600),
                "The battle was not over within 10 minutes. The party has to lose to the Rocket, and "
                "quickly: this program is meant for a single weak Pokemon."
            );
        }

        env.console.log("Talking to nurse joy...");
        wait_start = current_time();
        while (true){
            WhiteDialogWatcher nurse_joy_dialog(COLOR_RED);
            int ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 2000ms);
                },
                { nurse_joy_dialog }
            );

            if (ret == 0){
                break;
            }
            give_up_after(
                wait_start, std::chrono::seconds(60),
                "The nurse did not speak within 60 seconds of the battle's end."
            );
        }

        // Spam B till nurse joy stops talking
        wait_start = current_time();
        while (true){
            WhiteDialogWatcher nurse_joy_dialog(COLOR_RED);
            int ret = run_until<ProControllerContext>(
                env.console, context,
                [](ProControllerContext& context){
                    pbf_mash_button(context, BUTTON_B, 2000ms);
                },
                { nurse_joy_dialog }
            );

            if (ret != 0){
                break;
            }
            give_up_after(
                wait_start, std::chrono::seconds(60),
                "The nurse's talk did not end within 60 seconds."
            );
        }

        stats.nuggets++;
        env.update_stats();

        if (STOP_AFTER_CURRENT.should_stop()){
            break;
        }
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}


}
}
}

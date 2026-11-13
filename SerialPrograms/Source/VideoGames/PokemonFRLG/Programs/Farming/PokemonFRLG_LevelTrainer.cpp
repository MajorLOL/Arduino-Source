/*  Pokemon FRLG Level Trainer
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  The training is the AutoStory's (PokemonFRLG_AutoStoryActions.cpp):
 *
 *  -   At one of the story's training places, its GRIND step with another
 *      level: from the nurse to the grass, back and forth along the grass
 *      line, to the nurse when the one that fights is below half its HP (or
 *      every 20 battles, for the PP), until the level is reached. Ends in
 *      front of the nurse, healed.
 *
 *  -   Where the player stands: back and forth along a line of
 *      wild-encounter tiles from the player's tile. No nurse, so it stops
 *      when the one that fights is low on HP, out of PP or fainted.
 *
 *  Unlike the AutoStory, nothing is reloaded and retried here: an error
 *  stops the program where it is.
 *
 */

#include <string>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryActions.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMemory.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryScript.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTools.h"
#include "PokemonFRLG_LevelTrainer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


LevelTrainer_Descriptor::LevelTrainer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:LevelTrainer",
        Pokemon::STRING_POKEMON + " FRLG", "Level Trainer",
        "",
        "Train the first Pokemon of the party to a level, in the grass where the player stands or at one "
        "of the AutoStory's training places (from its nurse and back, healing on the way).",
        PanelQualityStanding::EXPERIMENTAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {
            //  Multi-line OCR (battle text).
            "PaddleOCRDetection",
        }
    )
{}

struct LevelTrainer_Descriptor::Stats : public StatsTracker{
    Stats()
        : battles(m_stats["Battles"])
        , level_ups(m_stats["Level-ups"])
        , shinies(m_stats["Shinies"])
    {
        m_display_order.emplace_back("Battles");
        m_display_order.emplace_back("Level-ups");
        m_display_order.emplace_back("Shinies", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& battles;
    std::atomic<uint64_t>& level_ups;
    std::atomic<uint64_t>& shinies;
};

std::unique_ptr<StatsTracker> LevelTrainer_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


namespace{

//  The story's GRIND step of a training place, by its label. (The same in
//  the story of every starter but for the level.)
const char* place_label(LevelTrainer::Place place){
    switch (place){
    case LevelTrainer::Place::HERE:                 return "";
    case LevelTrainer::Place::VIRIDIAN_ROUTE2:      return "GrindRoute2South";
    case LevelTrainer::Place::PEWTER_ROUTE2:        return "GrindForBrock";
    case LevelTrainer::Place::CERULEAN_ROUTE4:      return "GrindRoute4";
    case LevelTrainer::Place::CERULEAN_ROUTE24:     return "GrindRoute24";
    case LevelTrainer::Place::VERMILION_ROUTE11:    return "GrindRoute11";
    case LevelTrainer::Place::CELADON_ROUTE7:       return "GrindRoute7";
    case LevelTrainer::Place::FUCHSIA_ROUTE15:      return "GrindRoute15";
    case LevelTrainer::Place::CINNABAR_MANSION:     return "GrindMansion";
    case LevelTrainer::Place::INDIGO_VICTORY_ROAD:  return "GrindVictoryRoad";
    }
    return "";
}

const StoryStep* find_place(const StoryScript& script, const std::string& label){
    for (size_t i = 0; i < script.step_count; i++){
        const StoryStep& step = script.steps[i];
        if (step.type == StepType::GRIND && label == step.label){
            return &step;
        }
    }
    return nullptr;
}

const char* end_text(StoryTrainEnd end){
    switch (end){
    case StoryTrainEnd::LEVEL_REACHED:  return "The level is reached.";
    case StoryTrainEnd::LOW_HP:         return "Stopped: the Pokemon in party slot 1 is low on HP. Heal it and start again.";
    case StoryTrainEnd::OUT_OF_PP:      return "Stopped: no PP left for any of its moves. Heal it and start again.";
    case StoryTrainEnd::LEAD_FAINTED:   return "Stopped: the Pokemon in party slot 1 fainted.";
    case StoryTrainEnd::WHITED_OUT:     return "Stopped: the party fainted. The player is at the last Pokemon Center; the game is not saved.";
    case StoryTrainEnd::BATTLE_LIMIT:   return "Stopped: the number of battles asked for is done.";
    }
    return "";
}

}   //  namespace



LevelTrainer::LevelTrainer()
    : SETUP_TEXT(
        "<b>Experimental:</b> this program is new and has not been tested on a console yet. Watch its first "
        "runs, and save the game by hand before you start it.<br>"
        "<b>Setup:</b> as for the AutoStory. In the game options: Text Speed: Fast. Battle Scene: Off. "
        "Battle Style: Set. Button Mode: not L=A. The game in English (battle text and move names are read "
        "with English OCR). The Pokemon to train is the first of the party; it picks the move that does the "
        "most damage each turn and decides itself which new moves to learn.<br>"
        "<b>Where to start:</b> for \"where the player stands\", on a tile with wild Pokemon, with a "
        "straight line of such tiles the way the options say. For a training place, <b>in front of the "
        "nurse of that Pokemon Center, facing her</b>: the program first heals there (a whiteout then "
        "brings the player back to this nurse), walks to the grass, trains, and comes back to heal."
    )
    , PLACE(
        "<b>Where to train:</b><br>The training places are those of the AutoStory, with its routes from "
        "the nurse to the grass. They are made for a save that is past them in the story: the trainers "
        "on the way must be beaten already.",
        {
            {Place::HERE,                "here",                "Where the player stands (no nurse: stops when low on HP)"},
            {Place::VIRIDIAN_ROUTE2,     "viridian-route2",     "Viridian City: Route 2, south end (wild level 2 to 5)"},
            {Place::PEWTER_ROUTE2,       "pewter-route2",       "Pewter City: Route 2, north end (wild level 2 to 5)"},
            {Place::CERULEAN_ROUTE4,     "cerulean-route4",     "Cerulean City: Route 4 (wild level 6 to 12)"},
            {Place::CERULEAN_ROUTE24,    "cerulean-route24",    "Cerulean City: Route 24, over the Nugget Bridge (wild level 7 to 14)"},
            {Place::VERMILION_ROUTE11,   "vermilion-route11",   "Vermilion City: Route 11 (wild level 11 to 17)"},
            {Place::CELADON_ROUTE7,      "celadon-route7",      "Celadon City: Route 7 (wild level 17 to 22)"},
            {Place::FUCHSIA_ROUTE15,     "fuchsia-route15",     "Fuchsia City: Route 15 (wild level 22 to 30)"},
            {Place::CINNABAR_MANSION,    "cinnabar-mansion",    "Cinnabar Island: Pokemon Mansion 1F (wild level 26 to 36)"},
            {Place::INDIGO_VICTORY_ROAD, "indigo-victory-road", "Indigo Plateau: Victory Road 2F (wild level 34 to 48)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Place::HERE
    )
    , LEVEL(
        "<b>Train to level:</b><br>The program stops once the first Pokemon of the party has this level. "
        "Its level is read from its summary at the start and after each level-up.",
        LockMode::LOCK_WHILE_RUNNING,
        50, 2, 100
    )
    , LINE(
        "<b>Where the player stands: the line goes:</b><br>The program walks back and forth along a "
        "straight line of tiles with wild Pokemon, from the tile the player stands on.",
        {
            {Line::UP,    "up",    "Up"},
            {Line::DOWN,  "down",  "Down"},
            {Line::LEFT,  "left",  "Left"},
            {Line::RIGHT, "right", "Right"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Line::UP
    )
    , LINE_LENGTH(
        "<b>Where the player stands: tiles in the line:</b><br>The player's tile included. All of them "
        "free to walk on and with wild Pokemon (tall grass, a cave floor, water while surfing).",
        LockMode::LOCK_WHILE_RUNNING,
        3, 2, 5
    )
    , STOP_HP(
        "<b>Where the player stands: stop below HP (%):</b><br>There is no nurse on the way: the program "
        "stops when the HP bar of the one that fights is below this after a battle. It also stops when "
        "every move it read is out of PP, and when the Pokemon faints. 0: only then.",
        LockMode::LOCK_WHILE_RUNNING,
        35, 0, 90
    )
    , MAX_BATTLES(
        "<b>Where the player stands: stop after battles:</b><br>0: no limit.",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 100000
    )
    , CATCH_SHINIES(
        "<b>Catch shiny Pokemon:</b><br>When a wild Pokemon is shiny (its shiny sound plays as it "
        "appears), throw the best ball in the bag at it (Ultra, Great, then Poke Ball; never the "
        "Master Ball) until it is caught, it gets away or the balls run out, then go on training. "
        "<b>Needs the game's sound:</b> set the audio input of the capture card, game volume on.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , SAVE(
        "<b>Save the game:</b><br>At a training place: after every heal at the nurse. Where the player "
        "stands: once, when the program stops (not after a whiteout). Needs the POKeDEX in the start menu.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , FAST_BATTLES(
        "<b>Faster battles (experimental):</b><br>Notice the end of a battle by the screen fading to black, "
        "instead of waiting 6 seconds for nothing of a battle to show any more: about 5 seconds less per "
        "battle. See the same option of the AutoStory. Off: the battles end as in the AutoStory's test runs.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , SHORT_GRASS_LOOKS(
        "<b>Shorter looks in the grass (experimental):</b><br>End the look for a wild battle after a walk along "
        "the line as soon as the screen has not changed for 0.3 seconds, instead of after the whole \"Grass step "
        "check\": about one second less for each walk without a battle. See the same option of the AutoStory.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , RUNNING(
        "<b>Running (experimental):</b><br>Hold B along the line, twice as fast. Needs the Running Shoes, "
        "and a map that allows running. See the same option of the AutoStory.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , OVERLAY_NOTES(
        "<b>Progress on the video:</b><br>Show each battle's result and HP, the level, heals and saves in "
        "the log on the video (turn on \"Log\" above the video).",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , GRASS_STEP_CHECK_MS(
        "<b>Grass step check (ms):</b><br>After each walk along the line, how long to watch for a wild "
        "battle that starts at the last step. Too short and a battle can start after the next walk was "
        "already counted.",
        LockMode::LOCK_WHILE_RUNNING,
        1500, 600, 5000
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SETUP_TEXT);
    PA_ADD_OPTION(PLACE);
    PA_ADD_OPTION(LEVEL);
    PA_ADD_OPTION(LINE);
    PA_ADD_OPTION(LINE_LENGTH);
    PA_ADD_OPTION(STOP_HP);
    PA_ADD_OPTION(MAX_BATTLES);
    PA_ADD_OPTION(CATCH_SHINIES);
    PA_ADD_OPTION(SAVE);
    PA_ADD_OPTION(FAST_BATTLES);
    PA_ADD_OPTION(SHORT_GRASS_LOOKS);
    PA_ADD_OPTION(RUNNING);
    PA_ADD_OPTION(OVERLAY_NOTES);
    PA_ADD_OPTION(GRASS_STEP_CHECK_MS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void LevelTrainer::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    LevelTrainer_Descriptor::Stats& stats = env.current_stats<LevelTrainer_Descriptor::Stats>();

    env.log(PROGRAM_NAME + " build " + PROGRAM_BUILD + " (FRLG Level Trainer, experimental)", COLOR_BLUE);

    const Place place = PLACE;
    const int target = (int)(uint16_t)LEVEL;

    StoryRuntime rt(env.console, context);
    rt.standalone = true;
    rt.segment_id = "Level Trainer";
    rt.has_dex = true;
    rt.encounter_check = Milliseconds((int64_t)(uint16_t)GRASS_STEP_CHECK_MS);
    rt.trainer_check = rt.encounter_check + 1000ms;
    //  The walks are checked on the video: that is how the program knows
    //  where on the line a battle stopped the player.
    rt.verify_walking = true;
    rt.running = RUNNING;
    rt.save_after_heal = SAVE;
    rt.catch_shinies = CATCH_SHINIES;
    //  No team whose party slots have to be kept free.
    rt.party_order_final = true;
    rt.battle.fast_end = FAST_BATTLES;
    rt.short_grass_look = SHORT_GRASS_LOOKS && rt.verify_walking;
    rt.overlay_notes = OVERLAY_NOTES;
    if (CATCH_SHINIES){
        rt.shiny_watch.reset(new StoryShinyWatch(env.console, context));
        env.log("Listening for the shiny sound: shiny Pokemon are caught.", COLOR_BLUE);
    }
    if (FAST_BATTLES){
        env.log("\"Faster battles\" is on (experimental): the end of a battle is looked for at the fade to black.", COLOR_BLUE);
    }

    //  The numbers under the video follow the battles.
    uint64_t battles_shown = 0;
    uint64_t level_ups_shown = 0;
    uint64_t shinies_shown = 0;
    auto show = [&](){
        stats.battles += rt.wild_battles + rt.trainer_battles - battles_shown;
        stats.level_ups += rt.memory.level_ups - level_ups_shown;
        stats.shinies += rt.shinies - shinies_shown;
        battles_shown = rt.wild_battles + rt.trainer_battles;
        level_ups_shown = rt.memory.level_ups;
        shinies_shown = rt.shinies;
        env.update_stats();
    };
    rt.on_battle = show;

    story_check_memory_at_start(rt);
    story_memory_reset();
    home_black_border_check(env.console, context);

    std::string summary;
    if (place == Place::HERE){
        StoryTrainHere options;
        switch ((Line)LINE){
        case Line::UP:      options.forth = DPAD_UP; break;
        case Line::DOWN:    options.forth = DPAD_DOWN; break;
        case Line::LEFT:    options.forth = DPAD_LEFT; break;
        case Line::RIGHT:   options.forth = DPAD_RIGHT; break;
        }
        options.length = (int)(uint16_t)LINE_LENGTH;
        options.level = target;
        options.stop_hp = (double)(uint16_t)STOP_HP / 100.0;
        options.max_battles = (uint64_t)(uint32_t)MAX_BATTLES;
        options.run = RUNNING;

        int level = -1;
        const StoryTrainEnd end = story_train_here(rt, options, level);
        show();
        summary = std::string(end_text(end)) + " Level " +
            (level < 0 ? std::string("not read") : std::to_string(level)) + " after " +
            std::to_string(rt.wild_battles) + " battles.";
        if (SAVE && end != StoryTrainEnd::WHITED_OUT){
            story_save(rt, rt.has_dex);
            story_saved(rt, 0);
        }
    }else{
        const StoryScript& script = autostory_routes();
        const std::string label = place_label(place);
        const StoryStep* step = find_place(script, label);
        if (step == nullptr){
            throw InternalProgramError(
                &env.logger(), PA_CURRENT_FUNCTION,
                "The story has no training place " + label + "."
            );
        }
        env.log(
            "Training place " + label + ": the routes start in front of the nurse. Healing there first.",
            COLOR_BLUE
        );
        //  In front of the nurse, facing her. A heal first: a whiteout then
        //  comes back to this nurse, where the routes start.
        rt.facing = DPAD_UP;
        story_heal(rt, label, true);
        story_train_at(rt, script, *step, target);
        show();
        summary = "Training at " + label + " done after " + std::to_string(rt.wild_battles) +
            " battles. The player is in front of the nurse, healed.";
    }

    env.log(summary, COLOR_BLUE);
    story_note(rt, summary, COLOR_GREEN);
    if (rt.battle.fast_end){
        env.log(story_fast_end_summary(rt), COLOR_BLUE);
    }
    if (rt.short_grass_look){
        env.log(story_grass_look_summary(rt), COLOR_BLUE);
    }
    story_log_memory(rt, true);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, summary);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}


}
}
}

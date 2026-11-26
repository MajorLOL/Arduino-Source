/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryNewGame.h"
#include "Actions/PokemonLZA_AutoStoryWorldWalks.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;
using namespace std::chrono_literals;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        std::format("{} LZA", STRING_POKEMON), "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A from a new game through Main Mission 03 (checkpoints 01.00a through 03.11), "
        "choosing the selected starter. "
        "Checkpoint 01.00a starts a new game (delete the save first) from the HOME screen (its tile among the first 8; a running game is closed) "
        "and makes the language, look and name below, up to Confirm Choices. 01.00b starts on Confirm Choices (made by the program or by hand), "
        "checks them and confirms. "
        "Later checkpoints load the latest save first, unless that is turned off.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


namespace{

//  The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase entries;
        for (const StoryCheckpoint& checkpoint : builtin_story_data().checkpoints){
            entries.add_entry(StringSelectEntry(checkpoint.id, std::format("{} {}", checkpoint.id, checkpoint.title)));
        }
        return entries;
    }();
    return database;
}

void check_checkpoint_range(ConsoleHandle& console, size_t first, size_t last, size_t checkpoints){
    if (last >= checkpoints){
        throw UserSetupError(console, "The checkpoint selection is out of range: pick the start and end checkpoints again.");
    }
    if (first > last){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
}

const EnumDropdownDatabase<StarterChoice>& starter_database(){
    static const EnumDropdownDatabase<StarterChoice> database{
        {StarterChoice::CHIKORITA, "chikorita", "Chikorita"},
        {StarterChoice::TOTODILE,  "totodile",  "Totodile"},
        {StarterChoice::TEPIG,     "tepig",     "Tepig"},
    };
    return database;
}

//  The walk from 01.03's start to each starter (console B, 2026-11-20);
//  Chikorita: none, the plan's legs ([-66, 600] held). All three hold the
//  stick directly for 600 ms with no L recenter.
std::optional<StoryMove> starter_move(StarterChoice starter){
    switch (starter){
    case StarterChoice::TOTODILE:
        return StoryMove{.legs = {{.turn = -50, .walk = 600ms}}, .style = StoryWalkStyle::HELD};
    case StarterChoice::TEPIG:
        return StoryMove{.legs = {{.turn = -34, .walk = 600ms}}, .style = StoryWalkStyle::HELD};
    case StarterChoice::CHIKORITA:
        break;
    }
    return std::nullopt;
}

//  Before `run_from` (dash is not unlocked yet), each step's run legs walk.
void walk_before(std::vector<StoryCheckpoint>& checkpoints, std::string_view run_from, StoryEnv& story){
    for (StoryCheckpoint& checkpoint : checkpoints){
        const bool walk_only = checkpoint.id < run_from;
        for (StoryStep& step : checkpoint.steps){
            step.run = [&story, walk_only, run = std::move(step.run)](PlzaBackend& backend, Pokemon::RunState& state) mutable{
                story.walk_only = walk_only;
                return run(backend, state);
            };
        }
    }
}

const EnumDropdownDatabase<NewGameLanguage>& new_game_language_database(){
    static const EnumDropdownDatabase<NewGameLanguage> database{
        {NewGameLanguage::English,             "en",      "English"},
        {NewGameLanguage::SpanishSpain,        "es-ES",   "Espa\xc3\xb1ol (Espa\xc3\xb1" "a)"},
        {NewGameLanguage::SpanishLatinAmerica, "es-419",  "Espa\xc3\xb1ol (Latinoam\xc3\xa9rica)"},
        {NewGameLanguage::French,              "fr",      "Fran\xc3\xa7" "ais"},
        {NewGameLanguage::German,              "de",      "Deutsch"},
        {NewGameLanguage::Italian,             "it",      "Italiano"},
        {NewGameLanguage::Japanese,            "ja",      "Japanese"},
        {NewGameLanguage::Korean,              "ko",      "Korean (choices by hand, start at 01.00b)"},
        {NewGameLanguage::ChineseSimplified,   "zh-Hans", "Chinese (Simplified)"},
        {NewGameLanguage::ChineseTraditional,  "zh-Hant", "Chinese (Traditional)"},
    };
    return database;
}

//  Throws UserSetupError, before any press, if `choices` cannot play the new game.
void check_new_game_choices(ConsoleHandle& console, std::span<const StoryCheckpoint> checkpoints, const NewGameChoices& choices){
    if (checkpoints.empty() || !checkpoint_starts_without_save(checkpoints.front())){
        return;
    }
    if (const std::optional<std::string> problem = new_game_run_problem(checkpoints, choices)){
        throw UserSetupError(console, *problem);
    }
}

//  A new game's start screen proves the save is fresh, so it is checked
//  before any press, the connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (checkpoint_makes_new_game_choices(first_row)){
        require_new_game_home(console, context);
    }else if (checkpoint_starts_without_save(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

constexpr std::string_view ATTEMPTS_PER_CHECKPOINT_LABEL =
    "<b>Attempts per Checkpoint:</b><br>A failed checkpoint that started from a save reloads it and tries again, up to this many times in all.";

constexpr std::string_view LOAD_SAVE_FIRST_LABEL =
    "Load the save first: a run from a checkpoint after 01.00b loads the latest save before it starts. "
    "Off: it starts from wherever the game is, which must be that checkpoint's start.";

//  First checkpoint whose walks hold B to run; dash unlocks after Taunie's battle in 03.02b.
constexpr std::string_view RUN_FROM = "03.03";

//  Captures the failure screenshot while the game is still on screen, then
//  immediately suspends to Switch HOME so PLZA cannot be attacked or overwrite
//  the save while stopped.
[[noreturn]] void stop_with_error(
    ConsoleHandle& console, ProControllerContext& context,
    ErrorReportMode mode, std::string message
){
    const VideoSnapshot frame = console.video().snapshot();
    go_home(console, context);
    OperationFailedExceptionWithScreenshot::fire(mode, std::move(message), console, frame.frame);
}

//  A run after the new game starts from the latest save, not from wherever
//  the game is. Throws OperationFailedException if it does not load.
void load_first_save(ConsoleHandle& console, ProControllerContext& context, StoryGame& game, PlzaBackend& backend, const StoryCheckpoint& checkpoint){
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(game, backend, checkpoint);
    if (loaded){
        stop_with_error(
            console, context, ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", checkpoint.id, *loaded)
        );
    }
    console.log(std::format("AutoStory: loaded the latest save for {}.", checkpoint.id));
}

//  The same reading on this many frames in a row, within the budget.
constexpr size_t SIGNATURE_AGREEING_FRAMES = 3;
constexpr size_t SIGNATURE_FRAME_BUDGET = 20;
constexpr Milliseconds SIGNATURE_POLL = 500ms;

//  Nothing read when the frames never agree. Presses nothing.
StartSignature read_start_signature(ConsoleHandle& console, ProControllerContext& context){
    std::optional<StartSignature> last;
    size_t agreeing = 0;
    for (size_t frame = 0; frame < SIGNATURE_FRAME_BUDGET; frame++){
        context.wait_for(SIGNATURE_POLL);
        const VideoSnapshot snapshot = console.video().snapshot();
        const StartSignature read = snapshot ? read_start_screen(*snapshot.frame) : StartSignature{};
        agreeing = last == read ? agreeing + 1 : 1;
        last = read;
        if (agreeing >= SIGNATURE_AGREEING_FRAMES && read != StartSignature{}){
            return read;
        }
    }
    return StartSignature{};
}

//  On the overworld, also opens the full map for wild_zone and caches the pin
//  so the checkpoint's first map action does not immediately reopen the map.
StartSignature read_start_position(StoryEnv& story, PlzaBackend& backend){
    StartSignature read = read_start_signature(story.env.console, story.context);
    if (read != StartSignature{}){
        const std::optional<MapPoint> pin = read_pin_on_map(story, backend);
        story.cached_map_pin = pin;
        read.wild_zone = wild_zone_from_map(pin);
    }
    return read;
}

//  The checkpoint to play from: `first` or an earlier one, by where the player
//  stands. An unknown start throws "start_position_unknown": nothing runs
//  from the wrong place.
size_t checked_first_checkpoint(
    StoryEnv& story, PlzaBackend& backend,
    std::span<const StoryCheckpoint> checkpoints, size_t first, std::span<const CheckpointStart> starts
){
    ConsoleHandle& console = story.env.console;
    //  The new game's checkpoints, before any save.
    if (find_start(starts, checkpoints[first].id) == nullptr){
        console.log(std::format("AutoStory: {} has no start signature: running it, no start check", checkpoints[first].id));
        backend.log(std::format("[Boundary] {} start: unchecked", checkpoints[first].id));
        return first;
    }
    const StartSignature read = builtin_start_reads_map(checkpoints[first].id)
        ? read_start_position(story, backend)
        : read_start_signature(console, story.context);
    const std::optional<size_t> from = checkpoint_at(checkpoints, starts, first, read);
    if (!from){
        stop_with_error(
            console, story.context, ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory: start_position_unknown: the screen ({}) matches no checkpoint start up to {}; nothing was run",
                to_string(read), checkpoints[first].id
            )
        );
    }
    if (*from != first){
        console.log(std::format(
            "AutoStory: the save is at {}'s start ({}), not {}'s: running from {}", checkpoints[*from].id, to_string(read), checkpoints[first].id, checkpoints[*from].id
        ));
    }else{
        console.log(std::format("AutoStory: the screen ({}) is {}'s start", to_string(read), checkpoints[first].id));
    }
    backend.log(std::format("[Boundary] {} start: pinned by minimap", checkpoints[*from].id));
    return *from;
}

} // namespace


AutoStory::AutoStory()
    : RANGE("Checkpoints", checkpoint_database())
    , GAME_LANGUAGE(
        "<b>Game Language:</b><br>01.00a picks it on the new game's language list; 01.00b checks it on Confirm Choices. "
        "Korean: make the choices by hand up to Confirm Choices and start at 01.00b.",
        new_game_language_database(),
        LockMode::LOCK_WHILE_RUNNING,
        NewGameLanguage::English
    )
    , PLAYER_LOOK(
        "<b>Player Look:</b><br>01.00a picks it on the look picker: 1 to 4 along the top row, 5 to 8 along the bottom.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, 8
    )
    , PLAYER_NAME(
        /*is_password=*/false, "<b>Player Name:</b><br>Letters, digits and - / : ' , . ? ! (the keyboard's first page); at most 12 characters, 6 in Japanese and Chinese.",
        LockMode::LOCK_WHILE_RUNNING, /*default_value=*/"Kosmos", /*placeholder_text=*/"Kosmos"
    )
    , ATTEMPTS_PER_CHECKPOINT(std::string(ATTEMPTS_PER_CHECKPOINT_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/3, /*min_value=*/1, /*max_value=*/5)
    , LOAD_SAVE_FIRST(std::string(LOAD_SAVE_FIRST_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/true)
    , STARTER("<b>Starter:</b><br>The starter 01.03 picks.", starter_database(), LockMode::LOCK_WHILE_RUNNING, StarterChoice::CHIKORITA)
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RANGE);
    PA_ADD_OPTION(GAME_LANGUAGE);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(LOAD_SAVE_FIRST);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const NewGameChoices new_game{.language = GAME_LANGUAGE, .look = PLAYER_LOOK, .name = PLAYER_NAME};
    StoryEnv story{.env = env, .context = context, .battle_ai = BATTLE_AI, .new_game = new_game};
    AutosaveSightings sightings;
    PlzaConsoleHooks hooks = plza_console_hooks(story, sightings, starter_move(STARTER));
    StoryData story_data = load_plza_story(hooks);
    note_autosaves(story_data.checkpoints, sightings);
    std::vector<StoryCheckpoint>& checkpoints = story_data.checkpoints;
    walk_before(checkpoints, RUN_FROM, story);
    size_t first = RANGE.start_index();
    const size_t last = RANGE.end_index();
    check_checkpoint_range(env.console, first, last, checkpoints.size());
    check_new_game_choices(env.console, std::span<const StoryCheckpoint>(checkpoints).subspan(first, last - first + 1), new_game);
    connect_controller(env.console, context, checkpoints[first]);

    PlzaConsoleBackend backend(story);
    StoryGame game = make_plza_story_game(story, story_data, sightings);
    RunState state;
    CheckpointRunOptions run_options{
        .attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT, .reload_when_chained = story_data.reload_when_chained,
        .starts = story_data.starts,
        .read_start = [&story, &backend](const StartSignature& start){
            return start.wild_zone ? read_start_position(story, backend) : read_start_signature(story.env.console, story.context);
        },
    };
    if (!checkpoint_starts_without_save(checkpoints[first])){
        if (LOAD_SAVE_FIRST){
            load_first_save(env.console, context, game, backend, checkpoints[first]);
        }
        first = checked_first_checkpoint(story, backend, checkpoints, first, run_options.starts);
    }

    const std::optional<CheckpointFailure> failure = run_plza_checkpoints(
        game, std::span<StoryCheckpoint>(checkpoints).subspan(first, last - first + 1), run_options, backend, state, sightings
    );
    if (failure){
        stop_with_error(
            env.console, context, ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory stopped at {}: {}", failure->checkpoint_id, failure->failure)
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", checkpoints[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <cctype>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Strings/Unicode.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_SystemKeyboardTyping.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_IntroScreens.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryNewGame.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

constexpr size_t HOME_TILES_SEARCHED = 8;

//  Ends every refusal and failure of 01.00a.
constexpr std::string_view NEW_GAME_BY_HAND_HINT =
    "Make the new game's choices by hand up to \"Confirm Choices\", then start the run at 01.00b.";

constexpr size_t LANGUAGE_LIST_ROWS = 5;
constexpr size_t LANGUAGE_LIST_SIZE = 2 * LANGUAGE_LIST_ROWS;

constexpr size_t LOOK_COLUMNS = 4;
constexpr size_t LOOK_COUNT = 2 * LOOK_COLUMNS;

struct NewGameLanguageInfo{
    std::string_view tag;
    size_t list_entry = 0;
    size_t name_limit = 0;
    Language ocr_language = Language::None;
    std::string_view confirm_text;
    bool keyboard_supported = true;
};

const NewGameLanguageInfo& new_game_language_info(NewGameLanguage language){
    static const std::array<NewGameLanguageInfo, LANGUAGE_LIST_SIZE> table{{
        {.tag = "en",      .list_entry = 0, .name_limit = 12, .ocr_language = Language::English,            .confirm_text = "English"},
        {.tag = "es-ES",   .list_entry = 1, .name_limit = 12, .ocr_language = Language::Spanish,            .confirm_text = "Espa\xc3\xb1" "a)"},
        {.tag = "es-419",  .list_entry = 2, .name_limit = 12, .ocr_language = Language::Spanish,            .confirm_text = "Latino"},
        {.tag = "fr",      .list_entry = 3, .name_limit = 12, .ocr_language = Language::French,             .confirm_text = "Fran"},
        {.tag = "de",      .list_entry = 4, .name_limit = 12, .ocr_language = Language::German,             .confirm_text = "Deutsch"},
        {.tag = "it",      .list_entry = 5, .name_limit = 12, .ocr_language = Language::Italian,            .confirm_text = "Italiano"},
        {.tag = "ja",      .list_entry = 6, .name_limit = 6,  .ocr_language = Language::Japanese,           .confirm_text = "\xe6\x97\xa5\xe6\x9c\xac"},
        {.tag = "ko",      .list_entry = 7, .name_limit = 6,  .ocr_language = Language::Korean,             .confirm_text = "\xed\x95\x9c\xea\xb5\xad", .keyboard_supported = false},
        {.tag = "zh-Hans", .list_entry = 8, .name_limit = 6,  .ocr_language = Language::ChineseSimplified,  .confirm_text = "\xe7\xae\x80\xe4\xbd\x93"},
        {.tag = "zh-Hant", .list_entry = 9, .name_limit = 6,  .ocr_language = Language::ChineseTraditional, .confirm_text = "\xe7\xb9\x81\xe9\xab\x94"},
    }};
    const size_t index = (size_t)language;
    return table[index < table.size() ? index : 0];
}

//  One press on an intro screen.
enum class IntroKey{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,
};

//  One step toward (to_column, to_row), column first then row; nullopt when already there.
std::optional<IntroKey> grid_step(size_t from_column, size_t from_row, size_t to_column, size_t to_row){
    if (from_column != to_column){
        return from_column < to_column ? IntroKey::RIGHT : IntroKey::LEFT;
    }
    if (from_row != to_row){
        return from_row < to_row ? IntroKey::DOWN : IntroKey::UP;
    }
    return std::nullopt;
}

//  Why `choices` cannot be played; nullopt when they can.
std::optional<std::string> choices_problem(const NewGameChoices& choices){
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (choices.look < 1 || choices.look > LOOK_COUNT){
        return std::format("AutoStory: the player look must be 1 to {}, not {}.", LOOK_COUNT, choices.look);
    }
    if (choices.name.empty()){
        return std::string("AutoStory: the player name is empty.");
    }
    const size_t characters = utf8_to_utf32(choices.name).size();
    if (characters > info.name_limit){
        return std::format(
            "AutoStory: the player name \"{}\" has {} characters; the game takes at most {} in this language ({}).",
            choices.name, characters, info.name_limit, info.tag
        );
    }
    return std::nullopt;
}

//  The Switch keyboard typer types the first page only (letters by their capital).
bool on_keyboard(std::string_view name){
    const std::map<char, KeyboardEntryPosition>& keys = KEYBOARD_POSITIONS(KeyboardLayout::QWERTY);
    for (const char ch : name){
        if (!keys.contains((char)std::toupper((unsigned char)ch))){
            return false;
        }
    }
    return true;
}

std::optional<std::string> name_typing_problem(const NewGameChoices& choices){
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (!info.keyboard_supported){
        return std::format(
            "AutoStory: the program cannot type a name on the {} keyboard (Hangul).",
            info.tag
        );
    }
    if (!on_keyboard(choices.name)){
        return std::format(
            "AutoStory: the player name \"{}\" has a character that is not on the name keyboard's first page "
            "(letters, digits and - / : ' , . ? !).",
            choices.name
        );
    }
    return std::nullopt;
}


using FrameCheck = MoveOnlyFunction<bool(const ImageViewRGB32& frame)>;

//  The logos before the language list take about 15 s, with no press needed.
constexpr Milliseconds LAUNCH_LIMIT = 60s;

//  Faster taps on the language list are dropped.
constexpr Milliseconds LANGUAGE_TAP_RELEASE = 600ms;

//  One portrait per tilt.
constexpr Milliseconds LOOK_TILT_HOLD = 150ms;
constexpr Milliseconds LOOK_TILT_RELEASE = 500ms;

//  One + submits the name; Chinese keyboards first take one to "Accept" the typed text.
constexpr size_t SUBMIT_PRESSES = 3;
constexpr Milliseconds SUBMIT_WAIT = 5s;

VideoSnapshot snapshot(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    return console.video().snapshot();
}

bool wait_for_frame(ConsoleHandle& console, ProControllerContext& context, FrameCheck& check, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (true){
        const VideoSnapshot frame = snapshot(console, context);
        if (frame && check(frame)){
            return true;
        }
        if (current_time() >= deadline){
            return false;
        }
        context.wait_for(250ms);
    }
}

[[noreturn]] void fail(ConsoleHandle& console, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, std::format("AutoStory intro: {}", message), console);
}

void press(ProControllerContext& context, IntroKey key, Milliseconds release){
    switch (key){
    case IntroKey::UP:
        pbf_press_dpad(context, DPAD_UP, /*hold=*/80ms, release);
        break;
    case IntroKey::DOWN:
        pbf_press_dpad(context, DPAD_DOWN, /*hold=*/80ms, release);
        break;
    case IntroKey::LEFT:
        pbf_press_dpad(context, DPAD_LEFT, /*hold=*/80ms, release);
        break;
    case IntroKey::RIGHT:
        pbf_press_dpad(context, DPAD_RIGHT, /*hold=*/80ms, release);
        break;
    case IntroKey::A:
        pbf_press_button(context, BUTTON_A, /*hold=*/80ms, release);
        break;
    }
}

//  The D-pad does not move the look picker.
void tilt(ProControllerContext& context, IntroKey key){
    switch (key){
    case IntroKey::UP:
        pbf_move_left_joystick(context, /*position=*/{0, +1}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::DOWN:
        pbf_move_left_joystick(context, /*position=*/{0, -1}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::LEFT:
        pbf_move_left_joystick(context, /*position=*/{-1, 0}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    case IntroKey::RIGHT:
        pbf_move_left_joystick(context, /*position=*/{+1, 0}, LOOK_TILT_HOLD, LOOK_TILT_RELEASE);
        break;
    default:
        break;
    }
}

void wait_for_language_list(ConsoleHandle& console, ProControllerContext& context){
    FrameCheck shows = [](const ImageViewRGB32& frame){
        return read_language_list_cursor(frame).has_value();
    };
    if (!wait_for_frame(console, context, shows, LAUNCH_LIMIT)){
        fail(console, std::format("the language list did not show within {} s of the launch.", LAUNCH_LIMIT.count() / 1000));
    }
    console.log("AutoStory intro: the language list shows.");
}

void pick_language(ConsoleHandle& console, ProControllerContext& context, NewGameLanguage language){
    const NewGameLanguageInfo& info = new_game_language_info(language);
    for (size_t attempt = 0; attempt < 20; attempt++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<size_t> cursor = frame ? read_language_list_cursor(frame) : std::nullopt;
        if (!cursor){
            context.wait_for(250ms);
            continue;
        }
        if (*cursor == info.list_entry){
            console.log(std::format("AutoStory intro: language {} (entry {}): A.", info.tag, info.list_entry));
            press(context, IntroKey::A, 500ms);
            return;
        }
        if (*cursor >= LANGUAGE_LIST_SIZE){
            break;
        }
        const std::optional<IntroKey> step = grid_step(
            *cursor / LANGUAGE_LIST_ROWS, *cursor % LANGUAGE_LIST_ROWS,
            info.list_entry / LANGUAGE_LIST_ROWS, info.list_entry % LANGUAGE_LIST_ROWS
        );
        if (!step){
            break;
        }
        press(context, *step, LANGUAGE_TAP_RELEASE);
    }
    fail(console, std::format("the language list's highlight did not reach {}.", info.tag));
}

//  Settled = two reads in a row agree.
std::optional<size_t> read_settled_look(ConsoleHandle& console, ProControllerContext& context){
    std::optional<size_t> previous;
    for (size_t read = 0; read < 14; read++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<size_t> cursor = frame ? read_look_picker_cursor(frame) : std::nullopt;
        if (cursor && cursor == previous){
            return cursor;
        }
        previous = cursor;
        context.wait_for(150ms);
    }
    return std::nullopt;
}

//  The picker opens with no highlight, and the first tilt highlights a
//  portrait that varies from launch to launch, so tilt until one reads.
void pick_look(ConsoleHandle& console, ProControllerContext& context, size_t look){
    FrameCheck check = is_look_picker;
    if (!wait_for_frame(console, context, check, 20000ms)){
        fail(console, "the look picker did not show within 20 s of the language's A.");
    }
    size_t unread = 0;
    for (size_t attempt = 0; attempt < 24 && unread <= 6; attempt++){
        const std::optional<size_t> cursor = read_settled_look(console, context);
        if (!cursor){
            unread++;
            tilt(context, IntroKey::LEFT);
            continue;
        }
        if (*cursor == look){
            console.log(std::format("AutoStory intro: look {}: A.", look));
            press(context, IntroKey::A, 500ms);
            return;
        }
        if (*cursor < 1 || *cursor > LOOK_COUNT || look < 1 || look > LOOK_COUNT){
            break;
        }
        const std::optional<IntroKey> step = grid_step(
            (*cursor - 1) % LOOK_COLUMNS, (*cursor - 1) / LOOK_COLUMNS,
            (look - 1) % LOOK_COLUMNS, (look - 1) / LOOK_COLUMNS
        );
        if (!step){
            break;
        }
        tilt(context, *step);
    }
    fail(console, std::format("the look picker's highlight was not read on look {}.", look));
}

//  Presses while the keyboard slides in are dropped.
void wait_for_keyboard(ConsoleHandle& console, ProControllerContext& context){
    FrameCheck check = is_name_keyboard;
    if (!wait_for_frame(console, context, check, 20000ms)){
        fail(console, "the name keyboard did not show within 20 s of the look's A.");
    }
    context.wait_for(1000ms);
}

//  Simplified Chinese's keyboard opens in Pinyin; its English key switches to letters.
void switch_to_letters(ConsoleHandle& console, ProControllerContext& context){
    constexpr SystemKeyboardKey LETTERS_KEY{.row = 4, .col = 3};
    for (size_t attempt = 0; attempt < 16; attempt++){
        const VideoSnapshot frame = snapshot(console, context);
        const std::optional<SystemKeyboardKey> cursor = frame ? read_system_keyboard_cursor(frame) : std::nullopt;
        if (!cursor){
            context.wait_for(300ms);
            continue;
        }
        if (*cursor == LETTERS_KEY){
            console.log("AutoStory intro: keyboard: English (letters) key: A.");
            press(context, IntroKey::A, 400ms);
            return;
        }
        if (cursor->col == 11 || cursor->col > LETTERS_KEY.col){
            press(context, IntroKey::LEFT, 300ms);
        }else if (cursor->row < LETTERS_KEY.row){
            press(context, IntroKey::DOWN, 300ms);
        }else{
            press(context, IntroKey::RIGHT, 300ms);
        }
    }
    fail(console, "the keyboard's cursor did not reach the English (letters) key.");
}

void submit_name(ConsoleHandle& console, ProControllerContext& context){
    FrameCheck confirm_choices = [](const ImageViewRGB32& frame){
        return read_confirm_choices_cursor(frame).has_value();
    };
    for (size_t pressed = 1; pressed <= SUBMIT_PRESSES; pressed++){
        console.log(std::format("AutoStory intro: keyboard: + ({} of at most {}).", pressed, SUBMIT_PRESSES));
        pbf_press_button(context, BUTTON_PLUS, /*hold=*/80ms, /*release=*/500ms);
        if (wait_for_frame(console, context, confirm_choices, SUBMIT_WAIT)){
            console.log("AutoStory intro: Confirm Choices shows.");
            return;
        }
        const VideoSnapshot frame = snapshot(console, context);
        if (!frame || !is_name_keyboard(frame)){
            break;
        }
        console.log("AutoStory intro: keyboard: still open after +.");
    }
    if (!wait_for_frame(console, context, confirm_choices, 15000ms)){
        fail(console, "Confirm Choices did not show after the name's +.");
    }
}

void type_name(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    wait_for_keyboard(console, context);
    if (choices.language == NewGameLanguage::ChineseSimplified){
        switch_to_letters(console, context);
    }
    console.log(std::format("AutoStory intro: typing \"{}\".", choices.name));
    FastCodeEntry::type_on_system_keyboard(console, context, choices.name);
    submit_name(console, context);
}

std::string with_by_hand_hint(std::string message){
    if (message.ends_with(NEW_GAME_BY_HAND_HINT)){
        return message;
    }
    return std::format("{} {}", message, NEW_GAME_BY_HAND_HINT);
}

void play_intro_steps(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    if (const std::optional<std::string> problem = choices_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    if (const std::optional<std::string> problem = name_typing_problem(choices)){
        throw UserSetupError(console, *problem);
    }
    require_new_game_home(console, context);
    launch_plza_from_home(console, context);
    wait_for_language_list(console, context);
    pick_language(console, context, choices.language);
    pick_look(console, context, choices.look);
    type_name(console, context, choices);
}

} // namespace


void require_new_game_home(ConsoleHandle& console, ProControllerContext& context){
    HomeMenuDetector home(console, COLOR_RED);
    FrameCheck on_home = [&home](const ImageViewRGB32& frame){
        return home.detect_only(frame);
    };
    if (!wait_for_frame(console, context, on_home, 2000ms)){
        //  HomeMenuDetector misses a dimmed (idle) HOME; B wakes it and does nothing else there.
        console.log("AutoStory intro: HOME not detected; pressing B once to wake a dimmed screen.");
        pbf_press_button(context, BUTTON_B, /*hold=*/80ms, /*release=*/1000ms);
        if (!wait_for_frame(console, context, on_home, 2000ms)){
            throw UserSetupError(
                console,
                std::format(
                    "AutoStory: 01.00a starts a new game (no save) from the Switch HOME (PLZA's tile among the first {}). "
                    "It is not on screen; pressed B once to wake the screen, nothing else was pressed. {}",
                    HOME_TILES_SEARCHED, NEW_GAME_BY_HAND_HINT
                )
            );
        }
    }
    console.log("AutoStory intro: starting on the Switch HOME.");
}

std::optional<std::string> new_game_run_problem(std::span<const Pokemon::StoryCheckpoint> rows, const NewGameChoices& choices){
    if (std::optional<std::string> problem = choices_problem(choices)){
        return problem;
    }
    for (const Pokemon::StoryCheckpoint& row : rows){
        if (!checkpoint_makes_new_game_choices(row)){
            continue;
        }
        if (const std::optional<std::string> problem = name_typing_problem(choices)){
            return std::format("{} {} types the name. {}", *problem, row.id, NEW_GAME_BY_HAND_HINT);
        }
    }
    return std::nullopt;
}

void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    try{
        play_intro_steps(console, context, choices);
    }catch (OperationFailedException& e){
        throw OperationFailedException(e.error_report_mode(), with_by_hand_hint(e.message()));
    }catch (UserSetupError& e){
        throw UserSetupError(console, with_by_hand_hint(e.message()));
    }
}


namespace{

std::optional<size_t> read_confirm_choices_now(
    ConsoleHandle& console, ProControllerContext& context, Milliseconds window
){
    std::optional<size_t> cursor;
    FrameCheck shows = [&cursor](const ImageViewRGB32& frame){
        cursor = read_confirm_choices_cursor(frame);
        return cursor.has_value();
    };
    wait_for_frame(console, context, shows, window);
    return cursor;
}

void check_profile(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    const VideoSnapshot frame = snapshot(console, context);
    if (!frame){
        return;
    }
    const std::string name = read_confirm_choices_name(frame);
    if (name != choices.name){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory intro: Confirm Choices shows the name \"{}\", not \"{}\". Nothing was confirmed.", name, choices.name),
            console
        );
    }
    const NewGameLanguageInfo& info = new_game_language_info(choices.language);
    if (!OCR::ocr_language_available(info.ocr_language)){
        console.log(std::format("AutoStory intro: Confirm Choices: no OCR for {}; its language is not checked.", info.tag));
        return;
    }
    const std::string language = read_confirm_choices_language(frame, info.ocr_language);
    if (language.find(info.confirm_text) == std::string::npos){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory intro: Confirm Choices shows the language \"{}\", not {}. Nothing was confirmed.", language, info.tag),
            console
        );
    }
    console.log(std::format("AutoStory intro: Confirm Choices shows \"{}\" in {}.", name, info.tag));
}

} // namespace


size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context){
    const std::optional<size_t> cursor = read_confirm_choices_now(console, context, /*window=*/2000ms);
    if (!cursor){
        throw UserSetupError(
            console,
            "AutoStory: 01.00b starts on the new game's \"Confirm Choices\" screen, after the language, look "
            "and name (01.00a, or made by hand). It is not on screen; nothing was pressed."
        );
    }
    console.log(std::format("AutoStory intro: Confirm Choices on screen, row {} highlighted.", *cursor));
    return *cursor;
}

void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices){
    std::optional<size_t> cursor = require_confirm_choices(console, context);
    check_profile(console, context, choices);
    for (size_t press = 0; press < 6 && cursor; press++){
        if (*cursor == CONFIRM_CHOICES_READY_ROW){
            console.log("AutoStory intro: Confirm Choices: A on \"Ready!\".");
            pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/2000ms);
            context.wait_for_all_requests();
            return;
        }
        console.log("AutoStory intro: Confirm Choices: DOWN.");
        pbf_press_dpad(context, DPAD_DOWN, /*hold=*/160ms, /*release=*/500ms);
        cursor = read_confirm_choices_now(console, context, /*window=*/1000ms);
        if (cursor){
            console.log(std::format("AutoStory intro: Confirm Choices: the highlight is on row {}.", *cursor));
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "AutoStory intro: the Confirm Choices highlight was not read on \"Ready!\".",
        console
    );
}


namespace{

constexpr Milliseconds HOME_TILE_RELEASE = 500ms;

bool highlighted_tile_is_plza(ConsoleHandle& console, ProControllerContext& context){
    const VideoSnapshot frame = snapshot(console, context);
    return frame && is_home_tile_plza(frame);
}

} // namespace

void launch_plza_from_home(ConsoleHandle& console, ProControllerContext& context){
    go_home(console, context);
    const bool first_is_plza = highlighted_tile_is_plza(console, context);
    console.log(std::format(
        "AutoStory launch: HOME's first tile (the running game, if any) is {}; closing it.",
        first_is_plza ? "PLZA" : "another game"
    ));
    close_game_from_home(console, context);

    pbf_press_button(context, BUTTON_HOME, /*hold=*/160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    std::optional<size_t> tile;
    for (size_t index = 0; index < HOME_TILES_SEARCHED; index++){
        if (index > 0){
            pbf_press_dpad(context, DPAD_RIGHT, /*hold=*/80ms, HOME_TILE_RELEASE);
        }
        if (highlighted_tile_is_plza(console, context)){
            tile = index;
            break;
        }
    }
    if (!tile){
        throw UserSetupError(
            console,
            std::format(
                "AutoStory: Pokemon Legends: Z-A is not on HOME's first {} tiles. Play it once so its tile moves to the front.",
                HOME_TILES_SEARCHED
            )
        );
    }
    console.log(std::format("AutoStory launch: launching PLZA, HOME's tile {}.", *tile));
    //  Slot 0: the cursor stays on PLZA's tile.
    start_game_from_home(console, context, /*tolerate_update_menu=*/true, /*game_slot=*/0, /*user_slot=*/0);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_CodeEntryTools.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardCodeEntry.h"
#include "NintendoSwitch_SystemKeyboardTyping.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{

using namespace std::chrono_literals;


namespace{

//  Longer than the code entry delays: the next step reads the screen, so the cursor
//  animation and the capture latency must be over.
constexpr Milliseconds MOVE_RELEASE = 200ms;
constexpr Milliseconds WRAP_RELEASE = 400ms;    //  Wrapping around a side animates for longer.
constexpr Milliseconds SETTLE = 300ms;          //  Before reading the cursor after moving it.
constexpr Milliseconds SHIFT_RELEASE = 250ms;
constexpr Milliseconds KEY_RELEASE = 300ms;

//  Presses while the keyboard slides in are ignored, so the cursor is re-read each try.
constexpr size_t MAX_MOVE_TRIES = 10;

//  Reads of the text field after a key press, SETTLE apart, before giving up.
constexpr size_t MAX_FIELD_READS = 3;

//  The fewest white pixels a typed character adds to the field ("." or "'").
constexpr size_t MIN_CHARACTER_PIXELS = 10;


std::vector<KeyboardEntryPosition> text_keys(Logger& logger, const std::string& text, KeyboardLayout layout){
    const std::map<char, KeyboardEntryPosition>& positions = KEYBOARD_POSITIONS(layout);
    std::vector<KeyboardEntryPosition> keys;
    std::string missing;
    for (char ch : text){
        //  Letters are listed under their uppercase key.
        auto iter = positions.find((char)std::toupper((unsigned char)ch));
        if (iter == positions.end()){
            missing += ch;
        }else{
            keys.emplace_back(iter->second);
        }
    }
    if (!missing.empty()){
        throw UserSetupError(
            logger,
            std::format("System keyboard: \"{}\" has characters with no key on the keyboard: \"{}\".", text, missing)
        );
    }
    return keys;
}

Milliseconds button_hold(ProControllerContext& context){
    const ConsoleSettings& settings = ConsoleSettings::instance();
    const CodeEntryDelays delays = context->performance_class() == ControllerPerformanceClass::SerialPABotBase_Wired
        ? CodeEntryDelays(settings.CODEBOARD_ENTRY_SWITCH1_WIRED)
        : CodeEntryDelays(settings.CODEBOARD_ENTRY_SWITCH1_WIRELESS);
    return delays.hold;
}

DpadPosition move_direction(CodeEntryAction action){
    switch (action){
    case CodeEntryAction::NORM_MOVE_UP:
    case CodeEntryAction::WRAP_MOVE_UP:
        return DpadPosition::DPAD_UP;
    case CodeEntryAction::NORM_MOVE_RIGHT:
    case CodeEntryAction::WRAP_MOVE_RIGHT:
        return DpadPosition::DPAD_RIGHT;
    case CodeEntryAction::NORM_MOVE_DOWN:
    case CodeEntryAction::WRAP_MOVE_DOWN:
        return DpadPosition::DPAD_DOWN;
    case CodeEntryAction::NORM_MOVE_LEFT:
    case CodeEntryAction::WRAP_MOVE_LEFT:
        return DpadPosition::DPAD_LEFT;
    default:
        return DpadPosition::DPAD_NONE;
    }
}

//  Off the character grid (the right column or bottom row), one step back onto it,
//  then the cursor is read again.
std::vector<CodeEntryAction> path_toward(SystemKeyboardKey cursor, KeyboardEntryPosition target){
    if (cursor.col == 11){
        return {CodeEntryAction::NORM_MOVE_LEFT};
    }
    if (cursor.row == 4){
        return {CodeEntryAction::NORM_MOVE_UP};
    }
    std::vector<CodeEntryAction> path = keyboard_get_path({cursor.row, cursor.col}, target);
    std::erase_if(path, [](CodeEntryAction action){ return !is_move(action); });
    return path;
}

//  The moves as letters for the log, e.g. "DLL" or "R*" for a wrap to the right.
std::string path_to_string(const std::vector<CodeEntryAction>& path){
    std::string ret;
    for (CodeEntryAction action : path){
        switch (move_direction(action)){
        case DpadPosition::DPAD_UP:
            ret += 'U';
            break;
        case DpadPosition::DPAD_RIGHT:
            ret += 'R';
            break;
        case DpadPosition::DPAD_DOWN:
            ret += 'D';
            break;
        case DpadPosition::DPAD_LEFT:
            ret += 'L';
            break;
        default:
            ret += '?';
            break;
        }
        if (is_wrap(action)){
            ret += '*';
        }
    }
    return ret;
}

std::optional<SystemKeyboardKey> read_cursor(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    return read_system_keyboard_cursor(screen);
}

[[noreturn]] void throw_cursor_failure(
    VideoStream& stream,
    std::optional<SystemKeyboardKey> last, KeyboardEntryPosition target, char ch
){
    std::string message = last
        ? std::format("the cursor is at ({}, {}), not on '{}' at ({}, {})", last->row, last->col, ch, target.row, target.col)
        : std::format("the cursor could not be read while moving to '{}' at ({}, {})", ch, target.row, target.col);
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: {}, after {} tries.", message, MAX_MOVE_TRIES),
        stream
    );
}

void move_cursor(
    VideoStream& stream, ProControllerContext& context, Milliseconds hold,
    KeyboardEntryPosition target, char ch
){
    std::optional<SystemKeyboardKey> last;
    for (size_t attempt = 0; attempt < MAX_MOVE_TRIES; attempt++){
        const std::optional<SystemKeyboardKey> cursor = read_cursor(stream, context);
        if (!cursor){
            stream.logger().log(std::format("System keyboard: no cursor seen, try {}.", attempt + 1));
            pbf_wait(context, SETTLE);
            continue;
        }
        last = cursor;
        if (cursor->row == target.row && cursor->col == target.col){
            return;
        }
        const std::vector<CodeEntryAction> path = path_toward(*cursor, target);
        stream.logger().log(std::format(
            "System keyboard: cursor ({}, {}) -> '{}' ({}, {}): {}, try {}.",
            cursor->row, cursor->col, ch, target.row, target.col, path_to_string(path), attempt + 1
        ));
        for (CodeEntryAction action : path){
            pbf_press_dpad(context, move_direction(action), hold, is_wrap(action) ? WRAP_RELEASE : MOVE_RELEASE);
        }
        pbf_wait(context, SETTLE);
    }
    throw_cursor_failure(stream, last, target, ch);
}

size_t read_text_pixels(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    return screen ? system_keyboard_text_pixels(screen) : 0;
}

//  Types `ch` with the cursor on its key, and checks that the text field grew.
void press_key(VideoStream& stream, ProControllerContext& context, Milliseconds hold, char ch){
    const size_t before = read_text_pixels(stream, context);
    if (std::isupper((unsigned char)ch)){
        pbf_press_button(context, BUTTON_LCLICK, hold, SHIFT_RELEASE);
    }
    pbf_press_button(context, BUTTON_A, hold, KEY_RELEASE);
    size_t after = 0;
    for (size_t read = 0; read < MAX_FIELD_READS; read++){
        after = read_text_pixels(stream, context);
        if (after >= before + MIN_CHARACTER_PIXELS){
            return;
        }
        pbf_wait(context, SETTLE);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("System keyboard: pressed '{}', but the text field did not change ({} -> {} white pixels).", ch, before, after),
        stream
    );
}

}  // namespace



void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout
){
    const std::vector<KeyboardEntryPosition> keys = text_keys(stream.logger(), text, layout);
    const Milliseconds hold = button_hold(context);
    for (size_t c = 0; c < text.size(); c++){
        move_cursor(stream, context, hold, keys[c], text[c]);
        press_key(stream, context, hold, text[c]);
    }
    context.wait_for_all_requests();
}



}  // namespace FastCodeEntry
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

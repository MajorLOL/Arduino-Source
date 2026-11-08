/*  Pokemon FRLG Autostory New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <string>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryMemory.h"
#include "PokemonFRLG_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{


//  The naming screen keyboard (naming_screen.c, sKeyboardChars). SELECT
//  goes UPPER -> lower -> symbols -> UPPER; the screen starts on UPPER
//  with the cursor at the top left key. Rows wrap around (4 rows); the
//  last column is followed by the PAGE / BACK / OK buttons, which the
//  typing here never goes to (START does: the cursor to OK).
const char* const KEYBOARD[3][4] = {
    {"ABCDEF .", "GHIJKL ,", "MNOPQRS", "TUVWXYZ"},
    {"abcdef .", "ghijkl ,", "mnopqrs", "tuvwxyz"},
    {"01234", "56789", "!?\x01\x02/-", "\x03\x04\x05\x06'"},     //  (the others: gender signs, quotes)
};
const int KEYBOARD_COLUMNS[3] = {8, 8, 6};

struct KeyPosition{
    int page;
    int x;
    int y;
};

bool key_position(char c, KeyPosition& out){
    if (c == '\0'){
        return false;
    }
    for (int page = 0; page < 3; page++){
        for (int y = 0; y < 4; y++){
            const char* row = KEYBOARD[page][y];
            for (int x = 0; row[x] != '\0'; x++){
                if (row[x] == c){
                    //  The space and . , are on both letter pages: the
                    //  page the cursor is on is fine (found on UPPER here,
                    //  the typing below looks on its own page first).
                    out = {page, x, y};
                    return true;
                }
            }
        }
    }
    return false;
}

bool key_on_page(char c, int page, KeyPosition& out){
    for (int y = 0; y < 4; y++){
        const char* row = KEYBOARD[page][y];
        for (int x = 0; row[x] != '\0'; x++){
            if (row[x] == c){
                out = {page, x, y};
                return true;
            }
        }
    }
    return false;
}


//  The load menu with a save: the CONTINUE window (main_menu.c, tiles
//  (3, 1) to (27, 11)) white across, where without a save there is only
//  the small NEW GAME window at the top.
bool load_menu_has_save(const ImageViewRGB32& screen){
    ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return false;
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    auto white = [&](int x, int y){
        const size_t px = std::min(width - 1, (size_t)((x + 0.5) * sx));
        const size_t py = std::min(height - 1, (size_t)((y + 0.5) * sy));
        const uint32_t p = game.pixel(px, py);
        return ((p >> 16) & 0xff) > 200 && ((p >> 8) & 0xff) > 200 && (p & 0xff) > 200;
    };
    int count = 0;
    for (int x = 24; x < 120; x += 2){
        count += white(x, 57) ? 1 : 0;
        count += white(x, 71) ? 1 : 0;
    }
    return count >= 90;
}


bool has(const std::string& text, const char* words){
    return text.find(words) != std::string::npos;
}

//  Letters and digits only, lower case.
std::string simple(const std::string& text){
    std::string out;
    for (char c : text){
        if (std::isalnum((unsigned char)c)){
            out += (char)std::tolower((unsigned char)c);
        }
    }
    return out;
}

size_t edit_distance(const std::string& a, const std::string& b){
    std::vector<size_t> row(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++){
        row[j] = j;
    }
    for (size_t i = 1; i <= a.size(); i++){
        size_t diagonal = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b.size(); j++){
            const size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

//  The name in Oak's question, after "after" (the OCR reads the line in
//  lower case): the same as the name typed, but for one letter misread.
bool name_in_text(const std::string& text, const char* after, const std::string& name){
    const size_t at = text.find(after);
    if (at == std::string::npos){
        return false;
    }
    std::string rest = text.substr(at + std::string(after).size());
    const size_t end = rest.find_first_of(".?!");
    if (end != std::string::npos){
        rest = rest.substr(0, end);
    }
    const std::string read = simple(rest);
    const std::string want = simple(name);
    if (read == want){
        return true;
    }
    return want.size() >= 3 && edit_distance(read, want) <= 1;
}


void tap(ProControllerContext& context, Button button, Milliseconds after){
    pbf_press_button(context, button, 80ms, after);
}
void tap(ProControllerContext& context, DpadPosition direction, Milliseconds after){
    pbf_press_dpad(context, direction, 80ms, after);
}


//  On the naming screen: the name it starts with (a default one) away,
//  the name typed, OK.
void type_name(StoryRuntime& rt, const std::string& name){
    ProControllerContext& context = rt.context;
    rt.console.log("Typing the name \"" + name + "\".");
    for (int i = 0; i < 8; i++){
        tap(context, BUTTON_B, 220ms);
    }
    int page = 0;
    int cx = 0;
    int cy = 0;
    for (char c : name){
        KeyPosition key;
        if (!key_on_page(c, page, key) && !key_position(c, key)){
            continue;   //  (checked before: story_name_problem())
        }
        while (page != key.page){
            tap(context, BUTTON_MINUS, 900ms);     //  SELECT: the next page
            page = (page + 1) % 3;
            if (cx >= KEYBOARD_COLUMNS[page]){
                cx = KEYBOARD_COLUMNS[page] - 1;
            }
        }
        while (cx < key.x){
            tap(context, DPAD_RIGHT, 170ms);
            cx++;
        }
        while (cx > key.x){
            tap(context, DPAD_LEFT, 170ms);
            cx--;
        }
        const int down = ((key.y - cy) % 4 + 4) % 4;
        if (down <= 2){
            for (int i = 0; i < down; i++){
                tap(context, DPAD_DOWN, 170ms);
            }
        }else{
            for (int i = 0; i < 4 - down; i++){
                tap(context, DPAD_UP, 170ms);
            }
        }
        cy = key.y;
        tap(context, BUTTON_A, 350ms);
    }
    //  START: the cursor to OK (also where it goes by itself after the
    //  seventh letter), A: done.
    tap(context, BUTTON_PLUS, 600ms);
    tap(context, BUTTON_A, 300ms);
    context.wait_for_all_requests();
}

//  The naming screen (naming_screen.c): the keyboard on its light blue
//  panel (123, 173, 198) under the white box with the title, as on the
//  Switch (build 89: "RIVAL's NAME?" with AAAAAAA, typed by the A presses
//  that were meant for Oak's text).
bool is_naming_screen(const ImageViewRGB32& screen){
    ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return false;
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    auto pixel = [&](int x, int y){
        const size_t px = std::min(width - 1, (size_t)((x + 0.5) * sx));
        const size_t py = std::min(height - 1, (size_t)((y + 0.5) * sy));
        return game.pixel(px, py);
    };
    //  (Close: Oak's backdrop has (115, 180, 197).)
    int blue = 0;
    int total = 0;
    for (int y = 80; y < 140; y += 2){
        for (int x = 40; x < 160; x += 2){
            const uint32_t p = pixel(x, y);
            const int r = (p >> 16) & 0xff;
            const int g = (p >> 8) & 0xff;
            const int b = p & 0xff;
            total++;
            blue += std::abs(r - 123) < 6 && std::abs(g - 173) < 6 && std::abs(b - 198) < 8 ? 1 : 0;
        }
    }
    if (blue * 10 < total * 6){
        return false;
    }
    //  The yellow stripes of the background left and right of the panels.
    int yellow = 0;
    total = 0;
    for (int y = 60; y < 150; y++){
        for (int x : {2, 6, 10, 14, 226, 230, 234, 237}){
            const uint32_t p = pixel(x, y);
            total++;
            yellow += ((p >> 16) & 0xff) > 190 && ((p >> 8) & 0xff) > 180 && (p & 0xff) < 160 ? 1 : 0;
        }
    }
    if (yellow * 100 < total * 8){
        return false;
    }
    int white = 0;
    total = 0;
    for (int y = 18; y < 52; y += 2){
        for (int x = 50; x < 190; x += 2){
            const uint32_t p = pixel(x, y);
            total++;
            white += ((p >> 16) & 0xff) > 225 && ((p >> 8) & 0xff) > 225 && (p & 0xff) > 225 ? 1 : 0;
        }
    }
    return white * 100 >= total * 45;
}

std::string ocr_box(ConsoleHandle& console, const ImageFloatBox& box, uint32_t mins, uint32_t maxs){
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return "";
    }
    ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    ImageViewRGB32 image = extract_box_reference(game, box);
    ImageRGB32 bw = to_blackwhite_rgb32_range(image, true, mins, maxs);
    std::string text = OCR::ocr_read(Language::English, bw, OCR::PageSegMode::SINGLE_BLOCK);
    std::string lower;
    for (char c : text){
        lower += c == '\n' ? ' ' : (char)std::tolower((unsigned char)c);
    }
    return lower;
}

//  Oak's text box (the standard one at the bottom): dark grey text, and
//  in case the intro's font is coloured, blue text.
const ImageFloatBox INTRO_TEXT_BOX(0.080, 0.765, 0.840, 0.175);
std::string read_intro_text(StoryRuntime& rt){
    std::string text = ocr_box(rt.console, INTRO_TEXT_BOX, combine_rgb(0, 0, 0), combine_rgb(128, 128, 128));
    if (simple(text).size() >= 6){
        return text;
    }
    std::string blue = ocr_box(rt.console, INTRO_TEXT_BOX, combine_rgb(0, 0, 100), combine_rgb(140, 170, 255));
    return simple(blue).size() > simple(text).size() ? blue : text;
}

//  The naming screen's title: "YOUR NAME?" or "RIVAL's NAME?".
const ImageFloatBox NAMING_TITLE_BOX(0.25, 0.075, 0.60, 0.125);


bool try_new_game(StoryRuntime& rt, const std::string& player, const std::string& rival, bool& had_save){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;

    //  The title screen, the load menu: NEW GAME (below CONTINUE when
    //  there is a save; DOWN does nothing when NEW GAME is all there is).
    soft_reset(
        console, context,
        [&](ConsoleHandle& console, ProControllerContext& context){
            had_save = false;
            for (int attempt = 0; attempt < 6 && !had_save; attempt++){
                if (attempt > 0){
                    pbf_wait(context, 100ms);
                    context.wait_for_all_requests();
                }
                VideoSnapshot screen = console.video().snapshot();
                had_save = screen && load_menu_has_save(screen);
            }
            console.log(had_save
                ? "Load menu: there is a save (it is overwritten at the first save of the new game)."
                : "Load menu: no save on the cartridge."
            );
            pbf_press_dpad(context, DPAD_DOWN, 160ms, 600ms);
            context.wait_for_all_requests();
        }
    );
    console.log("NEW GAME: the intro.", COLOR_BLUE);

    //  The intro (oak_speech.c): A through the controls guide, the Pikachu
    //  pages and Oak's talk; "Are you a boy or a girl?": A, BOY. The naming
    //  screen comes for the player ("Let's begin with your name...") and,
    //  after "...Erm, what was his name now?" and A on NEW NAME (the first
    //  in the list), for the rival: it is told by its picture, not by Oak's
    //  text (A went on typing AAAAAAA there in build 89). Oak's questions
    //  "So your name is X." and "...Er, was it Y?" (YES/NO, the cursor on
    //  YES): A; NO when the name reads wrong. Then the bedroom.
    int player_typed = 0;
    int rival_typed = 0;
    bool player_checked = false;
    bool rival_checked = false;
    WallClock rival_time = WallClock::min();
    std::string last_text;
    int checks = 0;
    const WallClock intro_start = current_time();
    const WallClock deadline = intro_start + 8min;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        if (screen && is_naming_screen(screen)){
            pbf_wait(context, 600ms);       //  (the screen fading in)
            context.wait_for_all_requests();
            const std::string title = ocr_box(console, NAMING_TITLE_BOX, combine_rgb(0, 0, 0), combine_rgb(150, 150, 150));
            bool for_rival = title.find("rival") != std::string::npos;
            if (title.find("rival") == std::string::npos && title.find("your") == std::string::npos){
                //  Title not read: the first naming screen is the player's.
                for_rival = player_typed > 0;
            }
            console.log("Naming screen (title read as \"" + title + "\"): the " + (for_rival ? "rival's" : "player's") + " name.");
            type_name(rt, for_rival ? rival : player);
            if (for_rival){
                rival_typed++;
                rival_checked = false;
                rival_time = current_time();
            }else{
                player_typed++;
                player_checked = false;
            }
            pbf_wait(context, 2500ms);
            continue;
        }

        const std::string text = read_intro_text(rt);
        if (text != last_text && simple(text).size() >= 4){
            console.log("Intro text: \"" + text + "\"");
        }
        last_text = text;

        if (player_typed > 0 && !player_checked && has(text, "your name is")){
            const std::string full = read_intro_text(rt);
            pbf_wait(context, 800ms);       //  the YES/NO box
            context.wait_for_all_requests();
            player_checked = true;
            if (!name_in_text(full, "your name is", player) && player_typed < 2){
                console.log("The player's name read as \"" + full + "\": NO, typing it again.", COLOR_ORANGE);
                tap(context, DPAD_DOWN, 400ms);
                tap(context, BUTTON_A, 1500ms);
            }else{
                console.log("The player's name: YES.");
                tap(context, BUTTON_A, 1500ms);
            }
            continue;
        }
        if (rival_typed > 0 && !rival_checked && has(text, "was it") && !has(text, "rival")){
            const std::string full = read_intro_text(rt);
            pbf_wait(context, 800ms);
            context.wait_for_all_requests();
            rival_checked = true;
            if (!name_in_text(full, "was it", rival) && rival_typed < 2){
                console.log("The rival's name read as \"" + full + "\": NO, typing it again.", COLOR_ORANGE);
                tap(context, DPAD_DOWN, 400ms);
                tap(context, BUTTON_A, 1500ms);
            }else{
                console.log("The rival's name: YES.");
                tap(context, BUTTON_A, 1500ms);
            }
            continue;
        }
        if (has(text, "played with") || has(text, "time to go")){
            //  "ASH played with the NES. ...Okay! It's time to go!": A in the
            //  bedroom, facing the NES. The intro is over.
            console.log("The NES in the bedroom: the intro is over.");
            for (int i = 0; i < 3; i++){
                tap(context, BUTTON_B, 800ms);
            }
            context.wait_for_all_requests();
            if (in_overworld(console, context)){
                if (player_typed == 0 || rival_typed == 0){
                    console.log("In the bedroom, but the naming screens were not recognized: the new game again.", COLOR_RED);
                    return false;
                }
                console.log("In the bedroom.", COLOR_BLUE);
                return true;
            }
            continue;
        }
        if (rival_typed > 0 && (has(text, "let's go") || has(text, "lets go") || has(text, "adventures"))){
            //  The last page needs no A: the picture shrinks, the bedroom.
            console.log("Oak's last words: waiting for the bedroom.");
            for (int i = 0; i < 15; i++){
                pbf_wait(context, 2000ms);
                context.wait_for_all_requests();
                if (in_overworld(console, context)){
                    console.log("In the bedroom.", COLOR_BLUE);
                    return true;
                }
            }
            console.log("Not in the bedroom after the intro.", COLOR_RED);
            return false;
        }
        if (player_typed == 0 && current_time() > intro_start + 4min && ++checks % 6 == 0){
            //  No naming screen recognized in four minutes: the A presses
            //  may have got through the names and into the bedroom.
            if (in_overworld(console, context)){
                console.log("In the bedroom, but the naming screens were not recognized: the new game again.", COLOR_RED);
                return false;
            }
        }
        if (rival_typed > 0 && current_time() > rival_time + 12s && ++checks % 3 == 0){
            //  Oak's last words not read: the bedroom yet? (START does
            //  nothing in the intro.)
            if (in_overworld(console, context)){
                console.log("In the bedroom.", COLOR_BLUE);
                return true;
            }
        }
        tap(context, BUTTON_A, 1000ms);
    }
    console.log("The intro did not get to the bedroom in time.", COLOR_RED);
    return false;
}


}   //  namespace



std::string story_name_problem(const std::string& name){
    if (name.empty()){
        return "is empty";
    }
    if (name.size() > 7){
        return "is longer than 7 characters";
    }
    for (char c : name){
        KeyPosition key;
        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7f || !key_position(c, key)){
            return std::string("has a character the naming screen does not have: '") + c + "'";
        }
    }
    return "";
}


bool story_new_game(StoryRuntime& rt, const std::string& player, const std::string& rival){
    StoryMemoryScope memory_scope("new game");
    ConsoleHandle& console = rt.console;
    console.log("A new game: player \"" + player + "\", rival \"" + rival + "\".", COLOR_BLUE);
    bool had_save = false;
    for (int attempt = 0; attempt < 3; attempt++){
        if (attempt > 0){
            console.log("The new game again from the title screen (try " + std::to_string(attempt + 1) + ").", COLOR_ORANGE);
        }
        if (try_new_game(rt, player, rival, had_save)){
            //  The bedroom, where C0_01 starts: saved there (YES to "There
            //  is already a saved file...").
            story_save(rt, false);
            rt.facing = DPAD_UP;
            return had_save;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_new_game(): The new game did not get to the bedroom in three tries.",
        console
    );
}



}
}
}

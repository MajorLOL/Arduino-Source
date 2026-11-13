/*  Pokemon FRLG Box Namer
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  For each name of the list, in the PC's box view with the hand on the
 *  box's name (pokemon_storage_system: the box title):
 *
 *      A               the box menu: JUMP, WALLPAPER, NAME, CANCEL
 *      DOWN, DOWN, A   NAME: the naming screen, with an empty name
 *      the name        (naming_screen.c: SELECT changes the page, the D-pad
 *                      moves over the keys, A types the key)
 *      START, A        OK: back in the box, the hand still on its name
 *      RIGHT           the next box
 *
 *  Which keys to press for a name is worked out by
 *  NamingScreen::typing_steps(), which is tested off the console against
 *  the game's own rules for the keyboard; so are the readers of the screen
 *  (on pictures). What is in this file has only been compiled.
 *
 *  Nothing is pressed blind where a missed or a doubled press would do
 *  harm:
 *    - The arrow of the box menu is seen on NAME before A is pressed.
 *    - After every character the name is read back from the screen, each
 *      character against the game's own font (NamingScreen::read_entry):
 *      the typed characters, nothing after them, and the underscore of the
 *      next place where it should be (that is what tells a space). A
 *      doubled A is seen before it can fill the name and so put the cursor
 *      on OK, where the next A would save it.
 *    - A space in the 8th place cannot be told from an empty 8th place on
 *      the screen. It is typed over a visible character: that character in
 *      the 8th place is read first, which shows the name is full, and then
 *      the space that takes its place (a full name takes every further
 *      character in its last place).
 *    - OK is pressed only on a name that reads exactly as written.
 *    - Back in the box, its title is looked at: it must show the name,
 *      character by character. The Switch versions of the game pass a name
 *      through the console's list of words that are not allowed when OK is
 *      pressed (MainState_PressedOKButton, svc_BadWordCheck) and do not
 *      take one that is on it; the box then keeps the name it had, and the
 *      naming screen closes as it always does.
 *    - The box is seen to slide away after RIGHT (or to be another box
 *      after it), so that no name lands in the box of the one before it.
 *
 */

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_NamingScreenText.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_SelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_BoxNamer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


BoxNamer_Descriptor::BoxNamer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:BoxNamer",
        Pokemon::STRING_POKEMON + " FRLG", "Box Namer",
        "",
        "Give the PC's boxes the names of a list, one after the other, and read every character back "
        "from the screen: what an ACE code asks for. Start in the PC (Move " + Pokemon::STRING_POKEMON +
        "), in the first box to name, with an empty hand.",
        PanelQualityStanding::EXPERIMENTAL,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct BoxNamer_Descriptor::Stats : public StatsTracker{
    Stats()
        : boxes(m_stats["Boxes named"])
        , characters(m_stats["Characters typed"])
        , again(m_stats["Typed again"])
    {
        m_display_order.emplace_back("Boxes named");
        m_display_order.emplace_back("Characters typed");
        m_display_order.emplace_back("Typed again", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& boxes;
    std::atomic<uint64_t>& characters;
    std::atomic<uint64_t>& again;
};

std::unique_ptr<StatsTracker> BoxNamer_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


namespace{

using Stats = BoxNamer_Descriptor::Stats;

//  A box name (BOX_NAME_LENGTH).
const int NAME_LENGTH = 8;


[[noreturn]] void fail(SingleSwitchProgramEnvironment& env, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        message,
        env.console
    );
}

void tap(ProControllerContext& context, Button button, Milliseconds after){
    pbf_press_button(context, button, 80ms, after);
}
void tap(ProControllerContext& context, DpadPosition direction, Milliseconds after){
    pbf_press_dpad(context, direction, 80ms, after);
}
void pause(ProControllerContext& context, Milliseconds time){
    pbf_wait(context, time);
    context.wait_for_all_requests();
}


//  A look at the screen: the game's part of the video, by game pixel.
struct View{
    VideoSnapshot snapshot;
    ImageViewRGB32 game;
    NamingScreen::Pixel pixel;

    explicit operator bool() const{ return (bool)snapshot && (bool)pixel; }
};

NamingScreen::Pixel game_pixels(const ImageViewRGB32& game){
    return NamingScreen::game_pixels(game.data(), game.bytes_per_row(), game.width(), game.height());
}

View look(ConsoleHandle& console){
    View view;
    view.snapshot = console.video().snapshot();
    if (!view.snapshot){
        return view;
    }
    view.game = extract_box_reference(view.snapshot, GameSettings::instance().GAME_BOX);
    if (view.game.width() < 240 || view.game.height() < 160){
        return view;
    }
    view.pixel = game_pixels(view.game);
    return view;
}



//  ---- The box view ---------------------------------------------------------

//  How the box on the screen looks: its inside (the wallpaper and the
//  Pokemon in it, game pixels 96 to 228 by 52 to 148) and its title bar
//  left and right of the hand, as grey levels at every second game pixel.
//  Nothing in there moves while the hand is on the box's name: the hand
//  itself and the arrows next to the title are left out. (Measured on
//  captures of the Switch: Pokemon Automation's test pictures of the box.)
std::vector<uint8_t> box_look(const NamingScreen::Pixel& pixel){
    std::vector<uint8_t> out;
    auto add = [&](int left, int top, int right, int bottom){
        for (int y = top; y < bottom; y += 2){
            for (int x = left; x < right; x += 2){
                //  Four places in the game pixel: less of the video's noise.
                unsigned sum = 0;
                for (double sy : {0.3, 0.7}){
                    for (double sx : {0.3, 0.7}){
                        const uint32_t p = pixel(x + sx, y + sy);
                        sum += ((p >> 16) & 0xff) + ((p >> 8) & 0xff) + (p & 0xff);
                    }
                }
                out.push_back((uint8_t)(sum / 12));
            }
        }
    };
    add(96, 52, 228, 148);
    add(104, 20, 144, 34);
    add(180, 20, 216, 34);
    return out;
}

//  The mean difference between two looks, in grey levels.
double box_change(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b){
    if (a.empty() || a.size() != b.size()){
        return 255;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < a.size(); i++){
        sum += (uint64_t)std::abs((int)a[i] - (int)b[i]);
    }
    return (double)sum / (double)a.size();
}

//  The video's noise is 1 to 2 grey levels. A box that slides away, or a
//  menu over it, changes a third of it or more by tens of levels.
const double STILL = 4.5;
const double CHANGED = 9.0;

//  Stops when the box no longer looks as it did, two frames in a row: it is
//  sliding away (32 frames of 6 pixels, SetUpScrollToBox), or it is
//  another box already.
class BoxChangeWatcher : public VisualInferenceCallback{
public:
    BoxChangeWatcher(std::vector<uint8_t> before)
        : VisualInferenceCallback("BoxChangeWatcher")
        , m_before(std::move(before))
    {}

    virtual void make_overlays(VideoOverlaySet&) const override{}
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        if (timestamp == m_last){
            return false;
        }
        m_last = timestamp;
        ImageViewRGB32 game = extract_box_reference(frame, GameSettings::instance().GAME_BOX);
        if (game.width() < 240 || game.height() < 160){
            return false;
        }
        const double change = box_change(m_before, box_look(game_pixels(game)));
        m_most = std::max(m_most, change);
        if (change <= CHANGED){
            m_frames = 0;
            return false;
        }
        return ++m_frames >= 2;
    }

    double most() const{ return m_most; }

private:
    std::vector<uint8_t> m_before;
    WallClock m_last = WallClock::min();
    double m_most = 0;
    int m_frames = 0;
};

//  The box once it has looked the same for a moment: a fade or a slide is
//  over.
std::vector<uint8_t> still_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    std::vector<uint8_t> before;
    for (int i = 0; i < 24; i++){
        context.wait_for_all_requests();
        View view = look(env.console);
        std::vector<uint8_t> now;
        if (view){
            now = box_look(view.pixel);
        }
        if (!before.empty() && !now.empty() && box_change(before, now) < STILL){
            return now;
        }
        before = std::move(now);
        pause(context, 150ms);
    }
    return before;
}

//  The selection hand (Pokemon Automation's detector of the Box Sorter).
std::optional<BoxSelection> find_hand(ConsoleHandle& console){
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    return BoxSelectionModeDetector().detect(screen);
}

bool hand_on_box_name(SingleSwitchProgramEnvironment& env, ProControllerContext& context, Milliseconds timeout){
    const WallClock deadline = current_time() + timeout;
    while (true){
        context.wait_for_all_requests();
        const std::optional<BoxSelection> hand = find_hand(env.console);
        if (hand && hand->location == BoxSelectionLocation::BoxTile && !hand->closed){
            return true;
        }
        if (current_time() >= deadline){
            return false;
        }
        pause(context, 200ms);
    }
}

//  The box menu (AddMenu() in pokemon_storage_system: as wide as its
//  longest entry, WALLPAPER, plus 2 tiles; 2 tiles an entry; its bottom
//  right corner at tile (29, 15)): the window is at (144, 56) in game
//  pixels, and the arrow of entry k in the 8 by 16 pixels at
//  (144, 58 + 16 k). The same code places the menu of a Pokemon at
//  (152, 24), where the Box Sorter looks for its arrow; the box here is cut
//  around the arrow as it is there.
const int MENU_ENTRIES = 4;
const int MENU_NAME = 2;
ImageFloatBox menu_arrow_box(int entry){
    return ImageFloatBox((144 - 1.8) / 240.0, (58 + 16 * entry - 0.06) / 160.0, 0.0373, 0.0906);
}

//  The entry the box menu's arrow is at, -1 if there is no arrow.
int menu_arrow(ConsoleHandle& console){
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return -1;
    }
    for (int entry = 0; entry < MENU_ENTRIES; entry++){
        SelectionArrowDetector arrow(COLOR_RED, nullptr, menu_arrow_box(entry));
        if (arrow.detect(screen)){
            return entry;
        }
    }
    return -1;
}

int wait_for_menu_arrow(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int looks){
    for (int i = 0; i < looks; i++){
        context.wait_for_all_requests();
        const int entry = menu_arrow(env.console);
        if (entry >= 0){
            return entry;
        }
        pause(context, 200ms);
    }
    return -1;
}

//  To the next box: RIGHT on the box's name.
void next_box(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& what){
    const std::vector<uint8_t> before = still_box(env, context);
    if (before.empty()){
        fail(env, "No picture of the box (" + what + ").");
    }
    bool changed = false;
    for (int attempt = 0; attempt < 3 && !changed; attempt++){
        //  (Held, not pressed: HandleInput_BoxTitle looks at the held keys.)
        pbf_press_dpad(context, DPAD_RIGHT, 100ms, 0ms);
        context.wait_for_all_requests();
        BoxChangeWatcher watcher(before);
        const int ret = wait_until(env.console, context, 1500ms, { {watcher, 30ms} });
        if (ret == 0){
            changed = true;
            break;
        }
        //  Not seen to slide. Another box all the same?
        const std::vector<uint8_t> now = still_box(env, context);
        if (box_change(before, now) > STILL){
            changed = true;
            break;
        }
        env.log(
            "RIGHT on the box's name: the box did not change (" + what + "; the most it differed was " +
            std::to_string((int)(watcher.most() + 0.5)) + " grey levels). Once more.",
            COLOR_ORANGE
        );
    }
    if (!changed){
        fail(
            env,
            "The box does not change when RIGHT is pressed on its name (" + what + "). "
            "No name was typed for it."
        );
    }
    //  The slide takes half a second.
    pause(context, 700ms);
    still_box(env, context);
    if (!hand_on_box_name(env, context, 5s)){
        fail(env, "After RIGHT, the hand is no longer seen on the box's name (" + what + ").");
    }
}

//  From the box's name to the naming screen: A, the menu's NAME, A.
void open_naming_screen(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& what){
    const std::vector<uint8_t> before = still_box(env, context);

    int entry = -1;
    for (int attempt = 0; attempt < 2 && entry < 0; attempt++){
        tap(context, BUTTON_A, 500ms);
        entry = wait_for_menu_arrow(env, context, 10);
        //  The menu lies over more than half of the box: with it open, the
        //  box does not look as before.
        View view = look(env.console);
        const double change = view ? box_change(before, box_look(view.pixel)) : 255;
        if (entry >= 0 && change >= STILL){
            break;
        }
        //  No menu with its arrow. Only when the box looks as before did
        //  the press do nothing, and only then is it pressed again: a
        //  second A on an open menu would choose its first entry.
        entry = -1;
        if (change >= STILL){
            pbf_press_button(context, BUTTON_B, 80ms, 500ms);
            context.wait_for_all_requests();
            fail(
                env,
                "A on the box's name changed the screen, but the arrow of the box menu "
                "(JUMP, WALLPAPER, NAME, CANCEL) is not seen (" + what + "). B was pressed to close it."
            );
        }
        env.log("A on the box's name did not open its menu (" + what + "). Once more.", COLOR_ORANGE);
    }
    if (entry < 0){
        fail(env, "A on the box's name does not open the box menu (" + what + ").");
    }

    //  Down to NAME, a look after every press.
    for (int moves = 0; moves < 2 * MENU_ENTRIES && entry != MENU_NAME; moves++){
        tap(context, DPAD_DOWN, 300ms);
        entry = wait_for_menu_arrow(env, context, 5);
        if (entry < 0){
            break;
        }
    }
    if (entry != MENU_NAME){
        pbf_press_button(context, BUTTON_B, 80ms, 500ms);
        context.wait_for_all_requests();
        fail(
            env,
            "The arrow of the box menu did not get to NAME (" + what + "). B was pressed to close the menu."
        );
    }

    tap(context, BUTTON_A, 600ms);
    context.wait_for_all_requests();
    bool shown = false;
    for (int i = 0; i < 50 && !shown; i++){
        View view = look(env.console);
        shown = view && NamingScreen::keyboard_page(view.pixel) >= 0;
        if (!shown){
            pause(context, 150ms);
        }
    }
    if (!shown){
        fail(env, "NAME was chosen in the box menu, but the naming screen did not come (" + what + ").");
    }
    //  (The screen fading in.)
    pause(context, 700ms);
}



//  ---- The naming screen ----------------------------------------------------

//  One press on the naming screen, with the time the game needs after it
//  (the AutoStory types the player's and the rival's name with these).
void press(ProControllerContext& context, NamingScreen::Press what){
    switch (what){
    case NamingScreen::Press::A:        tap(context, BUTTON_A, 350ms); break;
    case NamingScreen::Press::B:        tap(context, BUTTON_B, 220ms); break;
    case NamingScreen::Press::SELECT:   tap(context, BUTTON_MINUS, 900ms); break;   //  (the page slides in)
    case NamingScreen::Press::START:    tap(context, BUTTON_PLUS, 500ms); break;
    case NamingScreen::Press::UP:       tap(context, DPAD_UP, 170ms); break;
    case NamingScreen::Press::DOWN:     tap(context, DPAD_DOWN, 170ms); break;
    case NamingScreen::Press::LEFT:     tap(context, DPAD_LEFT, 170ms); break;
    case NamingScreen::Press::RIGHT:    tap(context, DPAD_RIGHT, 170ms); break;
    }
}

const char* page_name(int page){
    switch (page){
    case NamingScreen::PAGE_UPPER:  return "UPPER";
    case NamingScreen::PAGE_LOWER:  return "lower";
    case NamingScreen::PAGE_OTHERS: return "others";
    default:                        return "none";
    }
}

//  The page the keyboard shows (a moment's patience: a page slides in).
int read_page(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    for (int i = 0; i < 8; i++){
        context.wait_for_all_requests();
        View view = look(env.console);
        if (view){
            const int page = NamingScreen::keyboard_page(view.pixel);
            if (page >= 0){
                return page;
            }
        }
        pause(context, 200ms);
    }
    return -1;
}

//  Whether the screen shows exactly "typed" in the name's field. If not,
//  "difference" says what it shows instead.
bool name_shows(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::u32string& typed, std::string& difference
){
    for (int i = 0; i < 3; i++){
        context.wait_for_all_requests();
        View view = look(env.console);
        if (view){
            const NamingScreen::Entry entry = NamingScreen::read_entry(view.pixel, NAME_LENGTH);
            difference = NamingScreen::entry_difference(entry, typed);
            if (difference.empty()){
                return true;
            }
            difference += "; the screen reads " + entry.summary();
        }else{
            difference = "no picture from the video";
        }
        pause(context, 250ms);
    }
    return false;
}

//  B until the name's field is empty.
void empty_the_name(ProControllerContext& context){
    for (int i = 0; i < NAME_LENGTH + 1; i++){
        press(context, NamingScreen::Press::B);
    }
    context.wait_for_all_requests();
}

//  Types the name into the naming screen. "fresh": the screen has just
//  opened (an empty name, the cursor on the first key). Otherwise the name
//  is emptied and the cursor put where it is known first. The presses are
//  those of NamingScreen::typing_steps() (tested against the game's own
//  rules for the keyboard); after each character the screen must show the
//  name so far, and after SELECT the page, before the next key is pressed.
//  False, with the reason, as soon as it does not.
bool type_name(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, Stats& stats,
    const std::u32string& name, bool fresh, std::string& problem
){
    NamingScreen::Keys keys;
    if (!fresh){
        for (NamingScreen::Press what : NamingScreen::restart_presses(NAME_LENGTH)){
            press(context, what);
        }
        keys.x = 0;
        keys.y = NamingScreen::ROWS - 1;
    }
    keys.page = read_page(env, context);
    if (keys.page < 0){
        problem = "the naming screen's keyboard is not seen";
        return false;
    }
    if (!name_shows(env, context, U"", problem)){
        problem = "the name does not start empty: " + problem;
        return false;
    }

    const std::vector<NamingScreen::TypingStep> steps = NamingScreen::typing_steps(name, NAME_LENGTH, keys);
    if (steps.empty()){
        problem = "the keyboard cannot type this name";
        return false;
    }
    for (const NamingScreen::TypingStep& step : steps){
        bool page_changed = false;
        for (NamingScreen::Press what : step.presses){
            if (page_changed && what != NamingScreen::Press::SELECT){
                //  Nothing is typed on a page that was not seen.
                const int page = read_page(env, context);
                if (page != step.page){
                    problem = std::string("after SELECT the keyboard is on the page ") + page_name(page) +
                        ", not " + page_name(step.page);
                    return false;
                }
                page_changed = false;
            }
            press(context, what);
            page_changed = page_changed || what == NamingScreen::Press::SELECT;
            if (what == NamingScreen::Press::A){
                stats.characters++;
            }
        }
        if (!name_shows(env, context, step.shows, problem)){
            problem = "the name should be " + NamingScreen::in_brackets(step.shows) + " now: " + problem;
            return false;
        }
    }
    return true;
}

//  Whether the title of the box on the screen shows the name. If not,
//  "seen" says how far it is from it.
bool title_shows(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::u32string& name, std::string& seen
){
    seen = "no picture from the video";
    for (int i = 0; i < 6; i++){
        context.wait_for_all_requests();
        View view = look(env.console);
        if (view){
            const NamingScreen::TitleLook title = NamingScreen::look_at_title(view.pixel, name);
            if (title.shows_name()){
                return true;
            }
            seen = std::to_string((int)(title.match * 100 + 0.5)) + " in 100 of the name's pixels are in the title";
            if (title.other_at >= 0){
                seen += ", and its place " + std::to_string(title.other_at + 1) + " looks like " +
                    NamingScreen::character_name(title.other);
            }
        }
        pause(context, 200ms);
    }
    return false;
}

//  The whole name of one box: type it (again from nothing if the screen
//  does not show it), OK, and back in the box.
void name_box(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, Stats& stats,
    const std::u32string& name, const std::string& what, bool check_title
){
    const std::string shown = NamingScreen::in_brackets(name);
    open_naming_screen(env, context, what);

    const int ATTEMPTS = 4;
    bool fresh = true;
    for (int attempt = 1; ; attempt++){
        std::string problem;
        const bool typed = type_name(env, context, stats, name, fresh, problem);
        fresh = false;
        if (typed){
            //  OK: START puts the cursor on it (it is there already after a
            //  full name), A presses it.
            press(context, NamingScreen::Press::START);
            tap(context, BUTTON_A, 600ms);
            context.wait_for_all_requests();
            int gone = 0;
            for (int i = 0; i < 30 && gone < 2; i++){
                View view = look(env.console);
                gone = view && NamingScreen::keyboard_page(view.pixel) < 0 ? gone + 1 : 0;
                if (gone < 2){
                    pause(context, 150ms);
                }
            }
            if (gone >= 2){
                break;
            }
            //  Still the naming screen: START or A did not come through,
            //  and an A without START types the key under the cursor.
            problem = "the naming screen stayed after START and A";
        }
        stats.again++;
        env.update_stats();
        if (attempt >= ATTEMPTS){
            //  Leave the box as it was: an empty name is not saved
            //  (SaveInputText).
            empty_the_name(context);
            std::string left;
            if (name_shows(env, context, U"", left)){
                press(context, NamingScreen::Press::START);
                tap(context, BUTTON_A, 600ms);
                context.wait_for_all_requests();
                left = " The name was emptied and OK pressed: the box keeps the name it had.";
            }else{
                left = " The naming screen was left as it is: press B until the name is empty, then START and A, "
                    "and the box keeps the name it had.";
            }
            fail(
                env,
                what + ": the name " + shown + " could not be typed so that the screen shows it (" +
                problem + ")." + left
            );
        }
        env.log(
            what + ": " + problem + ". Typing " + shown + " again from nothing (try " +
            std::to_string(attempt + 1) + " of " + std::to_string(ATTEMPTS) + ").",
            COLOR_ORANGE
        );
    }

    //  Back in the box, the hand on its name (InitCursorOnReopen).
    if (!hand_on_box_name(env, context, 10s)){
        fail(
            env,
            what + ": the name " + shown + " was typed and OK pressed, but the box with the hand on its name "
            "did not come back."
        );
    }
    if (check_title){
        still_box(env, context);
        std::string seen;
        if (!title_shows(env, context, name, seen)){
            fail(
                env,
                what + ": OK was pressed on " + shown + ", but the box's title does not show that name (" + seen +
                "). The game may not have taken it: the Switch versions pass a name through the console's list of "
                "words that are not allowed when OK is pressed, and the box keeps its old name if it is on it. "
                "Nothing more was typed. If the title does show the name, turn off \"Look at the box's title\" "
                "and report this."
            );
        }
    }
    stats.boxes++;
    env.update_stats();
    env.log(
        what + ": named " + shown + ", every character read back from the screen" +
        (check_title ? ", and the box's title shows it." : "."),
        COLOR_BLUE
    );
    env.console.overlay().add_log(what + ": " + shown, COLOR_WHITE);
}


}   //  namespace



BoxNamer::BoxNamer()
    : SETUP_TEXT(
        "<b>Experimental:</b> this program is new and has not been tested on a console yet. Watch its first run.<br>"
        "<b>What it does:</b> it gives the PC's boxes the names of the list below, starting with the box that is "
        "open: NAME in the box's menu, the name on the naming screen, OK, and on to the next box to the right. "
        "After every character it reads the name back from the screen and compares it, pixel by pixel, with the "
        "game's own font. A name that does not show exactly as written is typed again, and OK is only pressed "
        "on a name that does.<br>"
        "<b>Where to start:</b> in the PC (\"MOVE POK\u00E9MON\"), in the first box to name, with nothing in the hand. "
        "For the English games: the keyboards of the other languages differ.<br>"
        "<b>For an ACE code:</b> the program types names and knows nothing of the code. Compare the names with "
        "the code yourself before you use it; a code can damage the save."
    )
    , NAMES(
        "<b>Box names:</b><br>"
        "One on each line. A name has 1 to 8 characters of the game's keyboard: letters (capitals and small "
        "letters differ), digits, the space and <code>. , ! ? - / \u2026 \u201C \u201D \u2018 \u2019 \u2642 \u2640</code>. "
        "An underscore also stands for a space. Spaces at the ends of a line do not count: write "
        "underscores, or put the name between brackets, <code>[ FoHIoor]</code>.<br>"
        "Lines can be pasted as ACE codes are published, <code>Box  6: _ F o H I o o r [ FoHIoor]</code>: "
        "the name is what is between the brackets. With box numbers, the first line is for the box that is open "
        "and the boxes left out are passed over.",
        LockMode::LOCK_WHILE_RUNNING,
        "",
        "CCUnnREo\n[ FoHIoor]\nxn"
    )
    , CHECK_TITLE(
        "<b>Look at the box's title:</b><br>After OK, the title of the box must show the name. "
        "The Switch versions of the game do not take a name that is on the console's list of words that are "
        "not allowed: the box then keeps its old name. The program stops when the title does not show the name.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , CHECK_ONLY(
        "<b>Only check the list:</b><br>Read the list, write to the log how the names will be typed, "
        "and stop. Nothing is pressed.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SETUP_TEXT);
    PA_ADD_OPTION(NAMES);
    PA_ADD_OPTION(CHECK_TITLE);
    PA_ADD_OPTION(CHECK_ONLY);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void BoxNamer::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    Stats& stats = env.current_stats<Stats>();

    env.log(PROGRAM_NAME + " build " + PROGRAM_BUILD + " (FRLG Box Namer, experimental)", COLOR_BLUE);

    std::vector<NamingScreen::NameLine> lines;
    std::string error;
    if (!NamingScreen::parse_name_lines(NAMES, NAME_LENGTH, lines, error)){
        throw UserSetupError(env.logger(), "The list of box names: " + error);
    }
    const bool numbered = lines.front().box != 0;
    //  What each line's box is called in the log.
    std::vector<std::string> labels;
    for (size_t i = 0; i < lines.size(); i++){
        labels.push_back(
            numbered
                ? "Box " + std::to_string(lines[i].box)
                : "The box of line " + std::to_string(lines[i].line)
        );
        env.log(
            labels.back() + ": " + NamingScreen::in_brackets(lines[i].name) + " (" +
            std::to_string(lines[i].name.size()) + (lines[i].name.size() == 1 ? " character)" : " characters)")
        );
    }
    if (numbered){
        env.log(
            "The box that is open is taken to be box " + std::to_string(lines.front().box) +
            ": the program cannot see a box's number.",
            COLOR_BLUE
        );
    }else{
        env.log("The first name is for the box that is open, each next one for the next box to the right.", COLOR_BLUE);
    }
    if (CHECK_ONLY){
        env.log("The list is good: " + std::to_string(lines.size()) + " names. Nothing was pressed (\"Only check the list\").", COLOR_BLUE);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }

    home_black_border_check(env.console, context);
    pause(context, 1000ms);

    //  The hand: on the box's name, or in the box (START takes it to the
    //  name from there, HandleInput_InBox_Normal).
    std::optional<BoxSelection> hand = find_hand(env.console);
    if (!hand){
        throw UserSetupError(
            env.logger(),
            "The hand of the PC's box view is not seen. Start in the PC (MOVE POKeMON), in the first box to name."
        );
    }
    if (hand->closed){
        throw UserSetupError(env.logger(), "The hand holds a Pokemon. Put it down before starting the program.");
    }
    if (hand->location == BoxSelectionLocation::Grid){
        tap(context, BUTTON_PLUS, 500ms);
        context.wait_for_all_requests();
    }
    if (!hand_on_box_name(env, context, 3s)){
        throw UserSetupError(
            env.logger(),
            "The hand is not on the box's name, and START does not take it there. Start in the PC "
            "(MOVE POKeMON), in the first box to name, with the hand in the box or on its name."
        );
    }

    for (size_t i = 0; i < lines.size(); i++){
        if (i > 0){
            const int moves = numbered ? lines[i].box - lines[i - 1].box : 1;
            for (int move = 0; move < moves; move++){
                next_box(env, context, "on the way to " + labels[i]);
            }
        }
        name_box(env, context, stats, lines[i].name, labels[i], CHECK_TITLE);
    }

    env.log(
        "Done: " + std::to_string(lines.size()) + (lines.size() == 1 ? " box" : " boxes") +
        " named. Every character was read back from the screen before OK" +
        (CHECK_TITLE ? ", and every box's title shows its name. " : ". ") +
        "For an ACE code, compare the names with the code once more yourself before using it.",
        COLOR_BLUE
    );
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}


}
}
}

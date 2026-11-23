/*  Pokemon FRLG Autostory Box Scan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  StepType::BOXES (PocketPilot build 34; it ran on a console on 17 November
 *  2026: 134 Pokemon in 14 boxes read in 4 min 44 s, the hand seen every
 *  time): the player faces the PC of a Pokemon Center. Into BILL's PC and
 *  MOVE POKeMON, every cell of every box looked at, out again. What the
 *  rules of it are, and why nothing in the boxes can change:
 *  PokemonFRLG_AutoStoryBoxes.h.
 *
 *  Experimental (build 35: not run on a console when written).
 *  StepType::STONES: a Pokemon of a box changes places with the last one of
 *  the party, gets an evolution stone, and changes back. The presses, and
 *  why they cannot release a Pokemon: the end of PokemonFRLG_AutoStoryBoxes.h.
 *
 *  The hand, the cell under it and what the panel says of it are read with
 *  Pokemon Automation's own detectors of its Box Sorter
 *  (Inference/Box: BoxSelectionModeDetector, BoxCellOccupiedDetector,
 *  BoxCellReader). The Box Sorter's own scan (scan_box_data) is part of
 *  that program (its statistics) and changes boxes with L and R, which the
 *  game takes only with "Button mode: LR": here the boxes are changed on
 *  the box's name, with the D-pad.
 *
 */

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxCellOccupiedDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxCellReader.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryMemory.h"
#include "PokemonFRLG_AutoStoryBoxes.h"
#include "PokemonFRLG_AutoStoryDexCollection.h"
#include "PokemonFRLG_AutoStoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;

namespace{

using namespace AutoStoryBoxes;


[[noreturn]] void fail(StoryRuntime& rt, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        message,
        rt.console
    );
}

std::string upper(const std::string& slug){
    std::string name;
    for (char ch : slug){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    return name;
}

//  As Pokemon Automation's Box Sorter presses the D-pad in the box view.
void dpad(ProControllerContext& context, DpadPosition direction){
    pbf_press_dpad(context, direction, 80ms, 240ms);
}


//  How the box on the screen looks: its inside (the wallpaper and the
//  Pokemon in it: game pixels 96 to 228 by 52 to 148), as grey levels at
//  every second game pixel. Nothing in there moves while the hand is on the
//  box's name. (As the Box Namer looks at a box; on Pokemon Automation's
//  test pictures of the box view two captures of the same box differ by
//  less than 1 grey level on average, the same box with other Pokemon in it
//  by 7, two boxes by 20 to 70.)
std::vector<uint8_t> box_look(const ImageViewRGB32& screen){
    std::vector<uint8_t> out;
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return out;
    }
    for (int gy = 52; gy < 148; gy += 2){
        for (int gx = 96; gx < 228; gx += 2){
            //  Four places in the game pixel: less of the video's noise.
            unsigned sum = 0;
            for (double sy : {0.3, 0.7}){
                for (double sx : {0.3, 0.7}){
                    const size_t x = std::min(width - 1, (size_t)((gx + sx) * (double)width / 240.0));
                    const size_t y = std::min(height - 1, (size_t)((gy + sy) * (double)height / 160.0));
                    const uint32_t pixel = game.pixel(x, y);
                    sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
                }
            }
            out.push_back((uint8_t)(sum / 12));
        }
    }
    return out;
}

//  The mean difference between two looks, in grey levels.
double look_change(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b){
    if (a.empty() || a.size() != b.size()){
        return 255;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < a.size(); i++){
        sum += (uint64_t)std::abs((int)a[i] - (int)b[i]);
    }
    return (double)sum / (double)a.size();
}

//  The video's noise is 1 to 2 grey levels. A box that slides away, or
//  another box, changes a third of it or more by tens of levels.
const double STILL = 4.5;
const double CHANGED = 9.0;

std::vector<uint8_t> look_now(StoryRuntime& rt){
    rt.context.wait_for_all_requests();
    VideoSnapshot screen = rt.console.video().snapshot();
    if (!screen){
        return std::vector<uint8_t>();
    }
    return box_look(screen);
}

//  The box once it has looked the same for a moment: a fade or a slide is
//  over.
std::vector<uint8_t> still_look(StoryRuntime& rt){
    std::vector<uint8_t> before;
    for (int i = 0; i < 24; i++){
        std::vector<uint8_t> now = look_now(rt);
        if (!before.empty() && !now.empty() && look_change(before, now) < STILL){
            return now;
        }
        before = std::move(now);
        pbf_wait(rt.context, 150ms);
    }
    return before;
}

//  The selection hand (Pokemon Automation's detector of the Box Sorter),
//  looked for a few times: it moves up and down a little.
std::optional<BoxSelection> find_hand(StoryRuntime& rt, int looks){
    BoxSelectionModeDetector detector;
    for (int k = 0; k < looks; k++){
        rt.context.wait_for_all_requests();
        VideoSnapshot screen = rt.console.video().snapshot();
        if (screen){
            const std::optional<BoxSelection> hand = detector.detect(screen);
            if (hand){
                return hand;
            }
        }
        if (k + 1 < looks){
            pbf_wait(rt.context, 150ms);
        }
    }
    return std::nullopt;
}


//  ---- Into the box view, and out of it --------------------------------------

//  A in front of the PC, then by what the text box says
//  (AutoStoryBoxes::read_pc_text) to MOVE POKeMON and the box view. Ends
//  with the hand seen there.
void enter_box_view(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();

    const WallClock start = current_time();
    PcText last = PcText::NONE;
    WallClock pressed = WallClock::min();
    //  A was pressed on MOVE POKeMON: a fade, then the box view.
    bool entering = false;
    WallClock quiet_since = start;
    int others = 0;
    std::string other_text;
    while (current_time() - start < 90s){
        context.wait_for_all_requests();
        if (entering){
            const std::optional<BoxSelection> hand = find_hand(rt, 1);
            if (hand){
                console.log(label + ": the box view is open.");
                return;
            }
            if (current_time() - pressed < 2500ms){
                pbf_wait(context, 150ms);
                continue;
            }
        }
        const std::string text = story_read_dialog(rt);
        const PcText kind = read_pc_text(text);
        if (entering && (kind == PcText::NONE || kind == PcText::OTHER || kind == PcText::BOX_QUESTION)){
            //  The box view is on its way (what is read off it is not a
            //  text), but the hand is not seen yet.
            if (current_time() - pressed > 15s){
                break;
            }
            pbf_wait(context, 200ms);
            continue;
        }
        entering = false;
        if (kind == PcText::NONE){
            //  Between two texts; or the A brought nothing.
            if (current_time() - quiet_since > 12s){
                break;
            }
            pbf_wait(context, 250ms);
            continue;
        }
        quiet_since = current_time();
        if (kind == PcText::OTHER){
            //  A text of the PC while it still prints, or not the PC at
            //  all: a few looks tell.
            if (text != other_text){
                console.log(label + ": a text that is not (yet) one of the PC's: \"" + text + "\".");
            }
            other_text = text;
            if (++others >= 6){
                break;
            }
            pbf_wait(context, 400ms);
            continue;
        }
        others = 0;
        if (kind == last && current_time() - pressed < 1500ms){
            //  The press on this text is on its way.
            pbf_wait(context, 200ms);
            continue;
        }
        switch (pc_press_in(kind)){
        case PcPress::A:
            pbf_press_button(context, BUTTON_A, 160ms, 440ms);
            break;
        case PcPress::B:
            pbf_press_button(context, BUTTON_B, 160ms, 440ms);
            break;
        case PcPress::UP:
            pbf_press_dpad(context, DPAD_UP, 160ms, 340ms);
            break;
        case PcPress::DOWN:
            pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
            break;
        case PcPress::WAIT:
        case PcPress::FAIL:
            pbf_wait(context, 200ms);
            break;
        }
        if (kind != last){
            console.log(label + ": the PC: \"" + text + "\".");
        }
        entering = pc_enters_boxes(kind);
        last = kind;
        pressed = current_time();
    }
    context.wait_for_all_requests();
    fail(
        rt,
        label + ": the box view of the PC did not open" +
        (other_text.empty() ? std::string() : " (a text that is not the PC's: \"" + other_text + "\")") +
        ". Is the player in front of the PC, facing it?"
    );
}

//  B: "Continue BOX operations?"; B: no. The PC turns off, the storage
//  menu is back; B, and its list; B. Until the player is free to move.
void leave_box_view(StoryRuntime& rt){
    ProControllerContext& context = rt.context;
    pbf_press_button(context, BUTTON_B, 160ms, 840ms);
    pbf_press_button(context, BUTTON_B, 160ms, 2840ms);
    pbf_press_button(context, BUTTON_B, 160ms, 1340ms);
    pbf_press_button(context, BUTTON_B, 160ms, 1340ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "the PC");
}


//  ---- The hand in the box view ----------------------------------------------

//  The hand onto the box's name: UP from the top row of the cells, DOWN
//  from the buttons above it.
void to_box_name(StoryRuntime& rt, const std::string& label, bool holding = false){
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 5; attempt++){
        const std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (!hand){
            fail(rt, label + ": the hand of the box view is not seen (on the way to the box's name).");
        }
        if (hand->closed != holding){
            //  Holding one that the program did not pick up: not its doing,
            //  nothing more is pressed.
            fail(
                rt,
                label + (holding
                    ? ": the hand of the box view does not hold the Pokemon it picked up."
                    : ": the hand of the box view holds a Pokemon. Put it down by hand; the boxes are not read like this.")
            );
        }
        switch (hand->location){
        case BoxSelectionLocation::BoxTile:
            return;
        case BoxSelectionLocation::Grid:
            for (size_t k = 0; k <= hand->row; k++){
                dpad(context, DPAD_UP);
            }
            break;
        case BoxSelectionLocation::OpenPartyButton:
        case BoxSelectionLocation::CloseBoxButton:
            dpad(context, DPAD_DOWN);
            break;
        }
        pbf_wait(context, 200ms);
    }
    fail(rt, label + ": the hand of the box view did not get onto the box's name.");
}

//  From the box's name DOWN into the cells: the third one of the top row
//  (HandleInput_BoxTitle). Returns the cell the hand is seen on.
Cell into_cells(StoryRuntime& rt, const std::string& label, bool holding = false){
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 4; attempt++){
        dpad(context, DPAD_DOWN);
        pbf_wait(context, 200ms);
        const std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (hand && hand->location == BoxSelectionLocation::Grid){
            return Cell{(int)hand->row, (int)hand->column};
        }
        if (hand && hand->location != BoxSelectionLocation::BoxTile){
            //  On a button above the name: back onto the name first.
            to_box_name(rt, label, holding);
        }
    }
    fail(rt, label + ": the hand of the box view did not get from the box's name into the cells.");
}

//  The hand from the cell it is on to another one, seen there. The moves
//  are made blind and then looked at, as the Box Sorter does. A hand that
//  is not seen at all (rare: it moves a little, and what is behind it
//  matters) is taken to be where the moves put it; "unseen" counts those.
//  "holding": the hand holds a Pokemon the program picked up.
Cell move_to(
    StoryRuntime& rt, const std::string& label, Cell at, const Cell& target, bool at_seen, int& unseen,
    bool holding = false
){
    ProControllerContext& context = rt.context;
    if (at == target && at_seen){
        return at;
    }
    for (int attempt = 0; attempt < 5; attempt++){
        for (char move : cell_moves(at, target)){
            dpad(
                context,
                move == 'U' ? DPAD_UP : move == 'D' ? DPAD_DOWN : move == 'L' ? DPAD_LEFT : DPAD_RIGHT
            );
        }
        pbf_wait(context, 180ms);
        const std::optional<BoxSelection> hand = find_hand(rt, 3);
        if (!hand){
            if (++unseen > 20){
                fail(rt, label + ": the hand of the box view was not seen " + std::to_string(unseen) + " times.");
            }
            rt.console.log(
                label + ": the hand is not seen on row " + std::to_string(target.row + 1) + ", column " +
                std::to_string(target.column + 1) + ": taken to be there.",
                COLOR_ORANGE
            );
            return target;
        }
        if (hand->closed != holding){
            fail(
                rt,
                label + (holding
                    ? ": the hand of the box view does not hold the Pokemon it picked up."
                    : ": the hand of the box view holds a Pokemon. Put it down by hand; the boxes are not read like this.")
            );
        }
        if (hand->location != BoxSelectionLocation::Grid){
            //  It left the cells (a press too many at an edge): back in by
            //  the box's name.
            to_box_name(rt, label, holding);
            at = into_cells(rt, label, holding);
            continue;
        }
        at = Cell{(int)hand->row, (int)hand->column};
        if (at == target){
            return at;
        }
    }
    fail(
        rt,
        label + ": the hand of the box view did not get to row " + std::to_string(target.row + 1) + ", column " +
        std::to_string(target.column + 1) + "."
    );
}

//  On the box's name: RIGHT, the next box (the fourteenth: the first again);
//  LEFT, the one before. Returns the look of it once it stands still.
std::vector<uint8_t> next_box(
    StoryRuntime& rt, const std::string& label, const std::vector<uint8_t>& before,
    DpadPosition direction = DPAD_RIGHT
){
    ProControllerContext& context = rt.context;
    //  (Held, not pressed: HandleInput_BoxTitle looks at the held keys. The
    //  slide takes 32 frames, and nothing is read from the D-pad during it.)
    pbf_press_dpad(context, direction, 100ms, 0ms);
    bool slid = false;
    const WallClock pressed = current_time();
    while (current_time() - pressed < 1500ms){
        const std::vector<uint8_t> now = look_now(rt);
        if (!now.empty() && look_change(before, now) > CHANGED){
            slid = true;
            break;
        }
        pbf_wait(context, 50ms);
    }
    std::vector<uint8_t> after = still_look(rt);
    if (!slid && look_change(before, after) <= CHANGED){
        //  Two boxes that look alike (the same wallpaper, both empty), or a
        //  press the game did not take. Not pressed again: a box passed
        //  over is worse than one read twice, and the last look tells.
        rt.console.log(
            label + ": the box did not look any different after " + (direction == DPAD_LEFT ? "LEFT" : "RIGHT") +
            " on its name (two boxes alike, or the press was not taken).",
            COLOR_ORANGE
        );
    }
    return after;
}


//  ---- A Pokemon of a box and the last one of the party change places --------
//  (Build 35. The presses, and why none of them can release a Pokemon: the
//  end of PokemonFRLG_AutoStoryBoxes.h.)

//  The box on the screen (0: the one the PC was on), how that one looked
//  with the hand on its name, and how often the hand was not seen.
struct View{
    int shown = 0;
    int unseen = 0;
    std::vector<uint8_t> home;
};

std::string cell_text(int box, int cell){
    return "box " + std::to_string(box + 1) + ", cell " + std::to_string(cell + 1);
}

//  The view onto another box: the hand on the box's name, LEFT or RIGHT, the
//  shorter way round.
void show_box(StoryRuntime& rt, const std::string& label, View& view, int box){
    if (box == view.shown){
        return;
    }
    to_box_name(rt, label);
    std::vector<uint8_t> look = still_look(rt);
    const int right = ((box - view.shown) % BOX_COUNT + BOX_COUNT) % BOX_COUNT;
    const bool leftwards = right > BOX_COUNT / 2;
    const int presses = leftwards ? BOX_COUNT - right : right;
    for (int k = 0; k < presses; k++){
        look = next_box(rt, label, look, leftwards ? DPAD_LEFT : DPAD_RIGHT);
    }
    view.shown = box;
}

//  The cell the hand is on (from the box's name or a button: into the cells
//  first).
Cell hand_in_cells(StoryRuntime& rt, const std::string& label){
    const std::optional<BoxSelection> hand = find_hand(rt, 4);
    if (hand && hand->location == BoxSelectionLocation::Grid && !hand->closed){
        return Cell{(int)hand->row, (int)hand->column};
    }
    to_box_name(rt, label);
    return into_cells(rt, label);
}

//  What the panel on the left shows: the Pokemon under the hand, or the one
//  the hand holds (TrySetDisplayMonData: while one is held, the panel stays
//  on it).
struct Panel{
    bool pokemon = false;
    std::string slug;
    int level = -1;
};
Panel read_panel(StoryRuntime& rt){
    Panel panel;
    rt.context.wait_for_all_requests();
    VideoSnapshot screen = rt.console.video().snapshot();
    if (!screen){
        return panel;
    }
    BoxCellOccupiedDetector occupied;
    BoxCellReader reader;
    panel.pokemon = occupied.detect(screen);
    if (panel.pokemon){
        panel.slug = reader.read_name(rt.console.logger(), Language::English, screen);
        panel.level = reader.read_level(rt.console.logger(), screen);
    }
    return panel;
}
//  Looked at up to three times: the panel changes with a short effect.
Panel read_panel(StoryRuntime& rt, const std::string& expected){
    Panel panel;
    for (int look = 0; look < 3; look++){
        panel = read_panel(rt);
        if (panel.pokemon && panel.slug == expected){
            break;
        }
        pbf_wait(rt.context, 400ms);
    }
    return panel;
}
std::string panel_text(const Panel& panel){
    return !panel.pokemon ? std::string("nothing") : panel.slug.empty() ? std::string("a Pokemon whose name was not read") : upper(panel.slug);
}

//  A, A on the cell under the hand: the menu and its first entry (MOVE,
//  or PLACE with a Pokemon in the hand). A press that the game did not take
//  leaves the menu up, or not even that: A once more, never B (B on a cell
//  asks to leave the box view). "before": how the box looked before the
//  presses; a menu covers a third of it.
bool press_first_entry(
    StoryRuntime& rt, const std::string& label, View& view, const std::vector<uint8_t>& before,
    bool holding_after, const std::string& what
){
    ProControllerContext& context = rt.context;
    pbf_press_button(context, BUTTON_A, 160ms, 740ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1240ms);
    for (int more = 0; more < 3; more++){
        const std::vector<uint8_t> now = look_now(rt);
        const bool menu = !before.empty() && !now.empty() && look_change(before, now) > CHANGED;
        const std::optional<BoxSelection> hand = find_hand(rt, 3);
        if (!menu && (!hand || hand->closed == holding_after)){
            if (!hand){
                view.unseen++;
                rt.console.log(label + ": the hand is not seen after " + what + ": taken to be done.", COLOR_ORANGE);
            }
            return true;
        }
        if (more == 2){
            break;
        }
        rt.console.log(
            label + ": " + what + ": " + (menu ? "a menu is still up" : "the hand is as it was") + ". A once more.",
            COLOR_ORANGE
        );
        pbf_press_button(context, BUTTON_A, 160ms, 1240ms);
    }
    return false;
}

//  The Pokemon in a cell of a box and the last one of a party of six change
//  places. "in_cell": the species the list of the boxes has in the cell;
//  "in_party": the species of the party's last one. Both are read on the
//  screen: the first before anything is pressed, the second in the hand
//  after the change. Returns the level of the one that lies in the cell now
//  (-1: not read).
int swap_with_party(
    StoryRuntime& rt, const std::string& label, View& view,
    int box, int cell, const std::string& in_cell, const std::string& in_party
){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    const Cell target{cell / BOX_COLUMNS, cell % BOX_COLUMNS};
    const std::string where = cell_text(box, cell);

    show_box(rt, label, view, box);
    Cell at = hand_in_cells(rt, label);
    at = move_to(rt, label, at, target, true, view.unseen);

    //  What the list has there, read again before anything is pressed.
    const Panel lies = read_panel(rt, in_cell);
    if (!lies.pokemon || lies.slug != in_cell){
        if (lies.pokemon && !rt.out_watch.empty() && lies.slug == rt.out_watch){
            //  What the segment's trade gives lies where the one to give
            //  lay: the trade was made before. Nothing is pressed.
            console.log(
                label + ": " + where + " has " + upper(lies.slug) + " (the trade of this segment gives it), and the "
                "list of the boxes has " + upper(in_cell) + " there: the trade was made before. Nothing is changed.",
                COLOR_BLUE
            );
            rt.out_made_before = true;
            rt.out_made_level = lies.level;
            return lies.level;
        }
        fail(
            rt,
            label + ": " + where + " has " + panel_text(lies) + ", and the list of the boxes has " + upper(in_cell) +
            " there. The boxes are not as they were read: nothing is changed."
        );
    }
    console.log(
        label + ": " + where + ": " + upper(in_cell) + (lies.level > 0 ? " level " + std::to_string(lies.level) : std::string()) +
        ". It changes places with the party's last one (" + upper(in_party) + ")."
    );

    //  The hand picks it up.
    if (!press_first_entry(rt, label, view, still_look(rt), true, "picking up " + upper(in_cell))){
        fail(rt, label + ": the hand did not pick up " + upper(in_cell) + " (" + where + "). Nothing is saved: the game is reset.");
    }

    //  To the party: the left half of the bottom row, DOWN (the hand on PARTY
    //  POKeMON), A.
    at = move_to(rt, label, target, Cell{BOX_ROWS - 1, std::min(target.column, 2)}, true, view.unseen, true);
    dpad(context, DPAD_DOWN);
    pbf_wait(context, 300ms);
    {
        std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (hand && hand->location == BoxSelectionLocation::Grid){
            //  The press was not taken.
            dpad(context, DPAD_DOWN);
            pbf_wait(context, 300ms);
            hand = find_hand(rt, 4);
        }
        if (hand && hand->location == BoxSelectionLocation::CloseBoxButton){
            dpad(context, DPAD_LEFT);
            pbf_wait(context, 300ms);
            hand = find_hand(rt, 4);
        }
        if (!hand){
            view.unseen++;
            console.log(label + ": the hand is not seen below the cells: taken to be on PARTY POKeMON.", COLOR_ORANGE);
        }else if (hand->location != BoxSelectionLocation::OpenPartyButton){
            fail(rt, label + ": the hand did not get onto PARTY POKeMON. Nothing is saved: the game is reset.");
        }
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);     //  The party slides in: the hand on its last one.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);      //  The menu: SHIFT first.
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);     //  SHIFT: the two change places.
    pbf_press_button(context, BUTTON_B, 160ms, 1840ms);     //  The party slides away: the hand on the first cell.
    context.wait_for_all_requests();

    //  A Pokemon that holds a MAIL does not go out of the party's view in the
    //  hand: "Please remove the MAIL." (HandleInput, INPUT_HIDE_PARTY), and
    //  the party stays. (18 November, build 38: the JYNX of the trade, a FAB
    //  MAIL on it; the moves that were to take the hand back to its cell
    //  were made in the party.) The MAIL is taken off it before (the step
    //  of the trade); this tells it if one is still there.
    {
        const std::string text = story_read_dialog(rt);
        if (text.find("mail") != std::string::npos && (text.find("remove") != std::string::npos || text.find("please") != std::string::npos)){
            fail(
                rt,
                label + ": " + upper(in_party) + " holds a MAIL (\"" + text + "\"): the PC does not close the party with it in "
                "the hand. Nothing is saved: the game is reset."
            );
        }
    }

    //  What the hand holds now: the party's last one, as it was read before.
    const Panel holds = read_panel(rt, in_party);
    if (!holds.pokemon || holds.slug != in_party){
        fail(
            rt,
            label + ": after the change with the party the hand holds " + panel_text(holds) + ", not " +
            upper(in_party) + " (the party's last one). Nothing is saved: the game is reset."
        );
    }

    //  It goes where the other one was.
    Cell from{0, 0};
    bool from_seen = false;
    {
        const std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (hand && hand->location == BoxSelectionLocation::Grid){
            from = Cell{(int)hand->row, (int)hand->column};
            from_seen = true;
        }
    }
    at = move_to(rt, label, from, target, from_seen, view.unseen, true);
    if (!press_first_entry(rt, label, view, still_look(rt), false, "putting down " + upper(in_party))){
        fail(rt, label + ": the hand did not put down " + upper(in_party) + " (" + where + "). Nothing is saved: the game is reset.");
    }
    const Panel placed = read_panel(rt, in_party);
    if (!placed.pokemon || placed.slug != in_party){
        fail(
            rt,
            label + ": " + where + " has " + panel_text(placed) + " after the change, not " + upper(in_party) +
            ". Nothing is saved: the game is reset."
        );
    }
    console.log(
        label + ": " + upper(in_party) + " lies in " + where + " now, " + upper(in_cell) + " is the party's last one.",
        COLOR_BLUE
    );
    return placed.level;
}

//  The view back on the box the PC was on (where the game looks for room
//  for a caught Pokemon), and the box view left.
void back_home(StoryRuntime& rt, const std::string& label, View& view){
    show_box(rt, label, view, 0);
    to_box_name(rt, label);
    std::vector<uint8_t> look = still_look(rt);
    //  (One Pokemon of that box may be another one now: a few grey levels.)
    bool home = look_change(view.home, look) < CHANGED;
    for (int more = 0; !home && more < BOX_COUNT; more++){
        look = next_box(rt, label, look);
        home = look_change(view.home, look) < CHANGED;
    }
    if (!home){
        fail(
            rt,
            label + ": the box the PC was on did not come back on the screen. The game is not saved with the PC "
            "on another box: reset."
        );
    }
    if (view.unseen > 0){
        rt.console.log(label + ": the hand was not seen " + std::to_string(view.unseen) + " times.", COLOR_ORANGE);
    }
    leave_box_view(rt);
}

//  Into the box view, the change, the view back on the box the PC was on
//  and out.
int box_swap(
    StoryRuntime& rt, const std::string& label,
    int box, int cell, const std::string& in_cell, const std::string& in_party
){
    StoryMemoryScope memory_scope("PC boxes");
    enter_box_view(rt, label);
    View view;
    to_box_name(rt, label);
    view.home = still_look(rt);
    const int level = swap_with_party(rt, label, view, box, cell, in_cell, in_party);
    back_home(rt, label, view);
    return level;
}


//  ---- The party's last Pokemon into an empty cell (build 47) -----------------

//  From the party's first place down to its sixth: five presses.
constexpr int BOX_PARTY_LAST = 5;

//  Nothing on the panel, twice, with a little time between: the cell is
//  empty (the panel changes with a short effect).
bool cell_empty(StoryRuntime& rt){
    pbf_wait(rt.context, 500ms);
    for (int look = 0; look < 2; look++){
        if (read_panel(rt).pokemon){
            return false;
        }
        pbf_wait(rt.context, 400ms);
    }
    return true;
}

//  The hand goes to the first of the cells that is empty on the screen (the
//  list of the boxes may be older than the catches since), opens the party
//  with nothing in the hand, picks up the party's last Pokemon (MOVE) and
//  puts it down in that cell. "in_party": the species of the party's last
//  one, read before. Returns the level it has in the cell, and the cell.
//  "party_last": the presses of DOWN from the party's first place to its last
//  (5 for a party of six).
int store_last(
    StoryRuntime& rt, const std::string& label, View& view,
    const std::vector<std::pair<int, int>>& candidates, const std::string& in_party, int& box_found, int& cell_found,
    int party_last = BOX_PARTY_LAST
){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    int box = -1;
    int cell = -1;
    Cell target{0, 0};
    for (const std::pair<int, int>& candidate : candidates){
        show_box(rt, label, view, candidate.first);
        const Cell at = hand_in_cells(rt, label);
        target = Cell{candidate.second / BOX_COLUMNS, candidate.second % BOX_COLUMNS};
        move_to(rt, label, at, target, true, view.unseen);
        if (cell_empty(rt)){
            box = candidate.first;
            cell = candidate.second;
            break;
        }
        //  Something lies there (the list is older): the list takes it.
        const Panel there = read_panel(rt);
        console.log(
            label + ": " + cell_text(candidate.first, candidate.second) + " has " + panel_text(there) +
            ", the list of the boxes had nothing there: the next cell.",
            COLOR_ORANGE
        );
        rt.boxes.put(candidate.first, candidate.second, there.slug, there.level);
    }
    if (box < 0){
        fail(rt, label + ": none of the cells that were tried in the boxes is empty. Nothing is saved: the game is reset.");
    }
    const std::string where = cell_text(box, cell);
    console.log(label + ": " + where + " is empty. " + upper(in_party) + " (the party's last) goes there.");

    //  To the party: the left half of the bottom row, DOWN (the hand on PARTY
    //  POKeMON), A. With nothing in the hand the party opens on its first
    //  place.
    move_to(rt, label, target, Cell{BOX_ROWS - 1, std::min(target.column, 2)}, true, view.unseen);
    dpad(context, DPAD_DOWN);
    pbf_wait(context, 300ms);
    {
        std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (hand && hand->location == BoxSelectionLocation::Grid){
            dpad(context, DPAD_DOWN);
            pbf_wait(context, 300ms);
            hand = find_hand(rt, 4);
        }
        if (hand && hand->location == BoxSelectionLocation::CloseBoxButton){
            dpad(context, DPAD_LEFT);
            pbf_wait(context, 300ms);
            hand = find_hand(rt, 4);
        }
        if (!hand){
            view.unseen++;
            console.log(label + ": the hand is not seen below the cells: taken to be on PARTY POKeMON.", COLOR_ORANGE);
        }else if (hand->location != BoxSelectionLocation::OpenPartyButton){
            fail(rt, label + ": the hand did not get onto PARTY POKeMON. Nothing is saved: the game is reset.");
        }
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);     //  The party slides in: the hand on its first place.
    for (int k = 0; k < party_last; k++){
        dpad(context, DPAD_DOWN);
        pbf_wait(context, 150ms);
    }
    context.wait_for_all_requests();
    const Panel under = read_panel(rt, in_party);
    if (!under.pokemon || under.slug != in_party){
        fail(
            rt,
            label + ": the hand in the party is on " + panel_text(under) + ", not on " + upper(in_party) +
            " (the party's last). Nothing is saved: the game is reset."
        );
    }
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);      //  The menu: MOVE first.
    pbf_press_button(context, BUTTON_A, 160ms, 1240ms);     //  MOVE: the hand holds it.
    pbf_press_button(context, BUTTON_B, 160ms, 1840ms);     //  The party slides away: the hand on the first cell.
    context.wait_for_all_requests();

    //  A MAIL on it keeps the party in view (see swap_with_party()).
    {
        const std::string text = story_read_dialog(rt);
        if (text.find("mail") != std::string::npos && (text.find("remove") != std::string::npos || text.find("please") != std::string::npos)){
            fail(
                rt,
                label + ": " + upper(in_party) + " holds a MAIL (\"" + text + "\"): the PC does not close the party with it in "
                "the hand. Nothing is saved: the game is reset."
            );
        }
    }
    Cell from{0, 0};
    bool from_seen = false;
    {
        const std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (!hand || hand->location != BoxSelectionLocation::Grid || !hand->closed){
            fail(
                rt,
                label + ": the hand does not hold " + upper(in_party) + " in the cells after MOVE. Nothing is "
                "saved: the game is reset."
            );
        }
        from = Cell{(int)hand->row, (int)hand->column};
        from_seen = true;
    }
    move_to(rt, label, from, target, from_seen, view.unseen, true);
    if (!press_first_entry(rt, label, view, still_look(rt), false, "putting down " + upper(in_party))){
        fail(rt, label + ": the hand did not put down " + upper(in_party) + " (" + where + "). Nothing is saved: the game is reset.");
    }
    const Panel placed = read_panel(rt, in_party);
    if (!placed.pokemon || placed.slug != in_party){
        fail(
            rt,
            label + ": " + where + " has " + panel_text(placed) + " after putting down, not " + upper(in_party) +
            ". Nothing is saved: the game is reset."
        );
    }
    console.log(label + ": " + upper(in_party) + " lies in " + where + " now.", COLOR_BLUE);
    box_found = box;
    cell_found = cell;
    return placed.level;
}

}


bool story_boxes(StoryRuntime& rt, const std::string& label){
    StoryMemoryScope memory_scope("PC boxes");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(
        label + ": the PC's boxes are read, cell by cell (experimental). Nothing in them is changed: A is not "
        "pressed in the box view.",
        COLOR_BLUE
    );
    const WallClock start = current_time();

    enter_box_view(rt, label);

    BoxCellOccupiedDetector occupied(COLOR_BLUE);
    BoxCellReader reader;
    BoxSelectionModeDetector hand_detector;
    VideoOverlaySet overlays(console.overlay());
    hand_detector.make_overlays(overlays);
    occupied.make_overlays(overlays);
    reader.make_overlays(overlays);

    //  The box the PC was on: the first one read, and the one the view is
    //  left on.
    to_box_name(rt, label);
    const std::vector<uint8_t> first_look = still_look(rt);
    std::vector<uint8_t> look = first_look;

    BoxCensus census;
    int unseen = 0;
    for (int box = 0; box < BOX_COUNT; box++){
        story_note(rt, label + ": box " + std::to_string(box + 1) + " of " + std::to_string(BOX_COUNT));
        Cell at = into_cells(rt, label);
        bool at_seen = true;
        int in_box = 0;
        for (const Cell& cell : scan_order()){
            at = move_to(rt, label, at, cell, at_seen, unseen);
            at_seen = false;
            context.wait_for_all_requests();
            VideoSnapshot screen = console.video().snapshot();
            if (!screen){
                fail(rt, label + ": no picture of the box view.");
            }
            if (!occupied.detect(screen)){
                continue;
            }
            BoxPokemon one;
            one.box = box;
            one.cell = cell_number(cell);
            one.slug = reader.read_name(console.logger(), Language::English, screen);
            one.level = reader.read_level(console.logger(), screen);
            census.pokemon.push_back(one);
            in_box++;
        }
        census.boxes = box + 1;
        //  Cell by cell (row by row), not in the order the hand took.
        std::string names;
        for (const BoxPokemon& one : census.by_cell(box)){
            names += (names.empty() ? "" : ", ") +
                (one.slug.empty() ? std::string("(not read)") : upper(one.slug)) +
                (one.level > 0 ? " " + std::to_string(one.level) : std::string());
        }
        console.log(
            label + ": box " + std::to_string(box + 1) + " of " + std::to_string(BOX_COUNT) + ": " +
            (in_box == 0 ? std::string("empty.") : std::to_string(in_box) + " Pokemon: " + names + ".")
        );
        to_box_name(rt, label);
        look = next_box(rt, label, look);
    }

    //  Fourteen times RIGHT: the first box again. If it does not look like
    //  it, a press was not taken (or taken twice): the boxes after that one
    //  are not the ones their numbers say. RIGHT until the first box is back
    //  (the game looks for room for a caught Pokemon from the box the PC is
    //  on).
    census.closed = look_change(first_look, look) < STILL;
    bool home = census.closed;
    for (int more = 0; !home && more < BOX_COUNT; more++){
        look = next_box(rt, label, look);
        home = look_change(first_look, look) < STILL;
    }

    //  What was read is the program's from here, whatever comes after.
    rt.boxes = census;
    rt.boxes_read = census.closed;
    rt.boxes_known = census.closed;
    //  (Reading changes nothing in the game: this is how its save has them.)
    rt.boxes_at_save = census;
    rt.boxes_catches = rt.dex_catches;
    rt.boxes_readings++;

    const int seconds = (int)std::chrono::duration_cast<std::chrono::seconds>(current_time() - start).count();
    console.log(
        label + ": " + std::to_string(census.pokemon.size()) + " Pokemon in " + std::to_string(census.boxes_used()) +
        " of the " + std::to_string(BOX_COUNT) + " boxes (box 1: the one the PC was on), read in " +
        std::to_string(seconds / 60) + " min " + std::to_string(seconds % 60) + " s." +
        (census.unread() > 0
            ? " " + std::to_string(census.unread()) + " with a name that was not read (an EGG, or a nickname)."
            : std::string()) +
        (unseen > 0 ? " The hand was not seen " + std::to_string(unseen) + " times." : std::string()),
        COLOR_BLUE
    );
    if (!census.closed){
        console.log(
            label + ": after the last box the first one did not come back on the screen" +
            (home ? " (it did after more presses)" : "") +
            ": a press on the box's name was not taken, or taken twice. The boxes' numbers above may be off by one.",
            COLOR_RED
        );
    }
    if (!home){
        //  Not saved like this: the error resets the game.
        fail(
            rt,
            label + ": the box the PC was on did not come back on the screen. The game is not saved with the PC "
            "on another box: reset."
        );
    }

    leave_box_view(rt);
    return census.closed;
}


//  ---- Evolution stones (build 35, experimental) -----------------------------

std::vector<AutoStoryBoxes::TableRow> story_evolve_table(StoryRuntime& rt){
    std::vector<TableRow> table;
    if (rt.collection == nullptr){
        return table;
    }
    size_t count = 0;
    const AutoStoryDex::CollectionEntry* entries = AutoStoryDex::collection(count);
    const int game = rt.collection->game() == 1 ? 1 : 0;
    for (size_t k = 0; k < count; k++){
        const AutoStoryDex::CollectionEntry& entry = entries[k];
        const int needed = rt.collection->needed(entry.slug);
        if (needed < 0){
            continue;       //  Not in this game with this starter.
        }
        TableRow row;
        row.slug = entry.slug;
        row.name = entry.name;
        row.needed = needed;
        row.have = rt.collection->caught(entry.slug);
        row.how = entry.how[game];
        table.push_back(row);
    }
    return table;
}

std::vector<AutoStoryBoxes::StoneTarget> story_stone_targets(StoryRuntime& rt){
    return stone_targets_only(stone_targets(evolve_plan(story_evolve_table(rt), rt.boxes), rt.stones_out), rt.stones_only);
}

namespace{

//  The names of the targets that take a stone: "Nidoking, Clefable".
std::string names_with(const std::vector<StoneTarget>& targets, const std::string& stone){
    std::string names;
    for (const StoneTarget& target : targets){
        if (target.stone == stone){
            names += (names.empty() ? "" : ", ") + target.name;
        }
    }
    return names;
}

bool stone_in_bag(StoryRuntime& rt, const StoneKind& kind){
    return story_bag_has(rt, 0, -1, kind.words, kind.item);
}

}

std::vector<AutoStoryBoxes::StoneTarget> story_stones_not_sold(StoryRuntime& rt, const std::string& label){
    const std::vector<StoneTarget> targets = story_stone_targets(rt);
    for (const std::pair<std::string, int>& need : stones_needed(targets)){
        const StoneKind* kind = stone_kind(need.first);
        if (kind == nullptr || kind->mart >= 0 || rt.stones_in_bag.count(need.first) != 0){
            continue;
        }
        if (stone_in_bag(rt, *kind)){
            rt.stones_in_bag.insert(need.first);
        }else{
            rt.stones_out.insert(need.first);
            rt.console.log(
                label + ": no " + kind->item + " in the bag, and it is not sold: " + names_with(targets, need.first) +
                " not evolved.",
                COLOR_ORANGE
            );
        }
    }
    return story_stone_targets(rt);
}

std::vector<std::pair<std::string, int>> story_stones_to_buy(StoryRuntime& rt){
    const std::vector<StoneTarget> targets = story_stone_targets(rt);
    for (const std::pair<std::string, int>& need : stones_needed(targets)){
        const StoneKind* kind = stone_kind(need.first);
        if (kind == nullptr || kind->mart < 0 || rt.stones_in_bag.count(need.first) != 0){
            continue;
        }
        if (stone_in_bag(rt, *kind)){
            rt.stones_in_bag.insert(need.first);
        }
    }
    return stones_to_buy(targets, rt.stones_in_bag);
}

std::string story_stones_text(const std::vector<std::pair<std::string, int>>& stones){
    std::string text;
    for (const std::pair<std::string, int>& one : stones){
        const StoneKind* kind = stone_kind(one.first);
        text += (text.empty() ? "" : ", ") + std::to_string(one.second) + " " +
            (kind != nullptr ? kind->item : one.first.c_str());
    }
    return text;
}

void story_buy_stones(StoryRuntime& rt, const std::string& label){
    const std::vector<std::pair<std::string, int>> buy = stones_to_buy(story_stone_targets(rt), rt.stones_in_bag);
    if (buy.empty()){
        rt.console.log(label + ": no stone to buy.", COLOR_BLUE);
        return;
    }
    rt.console.log(label + ": buying " + story_stones_text(buy) + " (as far as the money goes).", COLOR_BLUE);
    for (const std::pair<std::string, int>& one : buy){
        const StoneKind* kind = stone_kind(one.first);
        if (kind == nullptr || kind->mart < 0){
            continue;
        }
        story_buy(rt, kind->mart, one.second, kind->item, 0, false);
        rt.stones_out.erase(one.first);
    }
}

int story_stones(StoryRuntime& rt, const std::string& label, const std::function<void()>& changed){
    ConsoleHandle& console = rt.console;
    //  The segment that fetched one stone: without it in the bag (the place
    //  was empty already, or the way did not end there) nothing is done.
    if (!rt.stones_only.empty()){
        const StoneKind* only = stone_kind(rt.stones_only);
        if (only == nullptr || !stone_in_bag(rt, *only)){
            console.log(
                label + ": no " + (only != nullptr ? std::string(only->item) : rt.stones_only) +
                " in the bag (it was not at the place, or taken before): nothing is evolved.",
                COLOR_ORANGE
            );
            return 0;
        }
        rt.stones_in_bag.insert(rt.stones_only);
    }
    //  The one that makes room: the party's sixth, when it is not of the team.
    std::string extra;
    const int extra_level = story_read_slot(rt, 6, &extra);
    if (extra_level < 0 || extra.empty()){
        console.log(
            label + ": party slot 6 has no Pokemon (or it was not read). A Pokemon of a box changes places with "
            "the sixth one of the party: with fewer in it, nothing is evolved.",
            COLOR_ORANGE
        );
        return 0;
    }
    for (const StoryTeamMember& member : rt.team){
        if (member.line.count(extra) != 0){
            console.log(
                label + ": party slot 6 is " + upper(extra) + ", one of the team: it is not put in a box. Nothing "
                "is evolved.",
                COLOR_ORANGE
            );
            return 0;
        }
    }
    console.log(
        label + ": the party's sixth one, " + upper(extra) + ", makes room: it waits in the cell of the Pokemon "
        "that gets a stone, and comes back after it (experimental).",
        COLOR_BLUE
    );

    int evolved = 0;
    while (true){
        const std::vector<StoneTarget> targets = story_stone_targets(rt);
        if (targets.empty()){
            break;
        }
        const StoneTarget target = targets.front();
        const StoneKind* kind = stone_kind(target.stone);
        if (kind == nullptr){
            break;
        }
        if (!stone_in_bag(rt, *kind)){
            rt.stones_out.insert(target.stone);
            rt.stones_in_bag.erase(target.stone);
            console.log(
                label + ": no " + kind->item + " in the bag" +
                (kind->mart >= 0 ? " (it was not bought: the money?)" : " (no more of them; it is not sold)") + ": " +
                names_with(targets, target.stone) + " not evolved.",
                COLOR_ORANGE
            );
            continue;
        }
        const std::string from = upper(target.from_slug);
        const std::string where = cell_text(target.box, target.cell);
        story_note(rt, label + ": " + target.name);
        console.log(
            label + ": " + target.name + ": " + from + " (" + where + ") gets a " + kind->item + ".",
            COLOR_BLUE
        );
        //  Into the party, the stone, a look at what it is now, and back.
        box_swap(rt, label, target.box, target.cell, target.from_slug, extra);
        story_use_item(rt, 6, kind->words, std::string(kind->item) + " on " + from);
        std::string now;
        const int level = story_read_slot(rt, 6, &now);
        if (now != target.slug){
            fail(
                rt,
                label + ": party slot 6 is " + (now.empty() ? std::string("not read") : upper(now)) + " after the " +
                kind->item + ", not " + upper(target.slug) + ". Nothing is saved: the game is reset."
            );
        }
        box_swap(rt, label, target.box, target.cell, extra, target.slug);

        //  One more in the POKeDEX table, the game saved, then the list.
        rt.caught.insert(target.slug);
        if (rt.collection != nullptr){
            rt.collection->add_caught(target.slug, false, false);
        }
        rt.dex_new_since_load.insert(target.slug);
        rt.boxes.put(target.box, target.cell, target.slug, level > 0 ? level : target.level);
        story_save(rt, true);
        story_saved(rt, rt.step_index);
        evolved++;
        console.log(
            "Evolved for the POKeDEX: " + target.slug + " (" + from + " with a " + kind->item + "). It lies in " +
            where + "; " + upper(extra) + " is back in the party.",
            COLOR_BLUE
        );
        story_note(rt, "Evolved: " + target.slug, COLOR_GREEN);
        if (changed){
            changed();
        }
        //  The table has it now: it is not looked for again. (If it were,
        //  the next stone of the kind would go to another one of the same
        //  species.)
        bool again = false;
        for (const StoneTarget& next : story_stone_targets(rt)){
            again |= next.slug == target.slug;
        }
        if (again){
            console.log(
                label + ": the POKeDEX table did not take " + target.slug + ": no more stones are used in this visit.",
                COLOR_RED
            );
            break;
        }
    }
    console.log(
        label + ": " + std::to_string(evolved) + " evolved with a stone in this visit.",
        COLOR_BLUE
    );
    return evolved;
}


//  ---- A Pokemon of a box into the party for a while (build 36) -------------

bool story_box_out(
    StoryRuntime& rt, const std::string& label, const std::string& give, const std::string& trade_get
){
    ConsoleHandle& console = rt.console;
    rt.out_made_before = false;
    rt.out_made_level = -1;
    rt.out_watch.clear();
    rt.out_box = -1;
    rt.out_cell = -1;
    rt.out_extra.clear();
    rt.out_give.clear();
    rt.out_now.clear();
    //  One of that species: the lowest level, and not the one that is set
    //  aside to evolve into a species the table still wants.
    const std::vector<EvolveLine> plan = evolve_plan(story_evolve_table(rt), rt.boxes);
    const std::vector<BoxPokemon> ones = rt.boxes.of(give);
    const BoxPokemon* pick = nullptr;
    for (size_t k = ones.size(); k-- > 0;){
        bool aside = false;
        for (const EvolveLine& line : plan){
            aside |= line.box == ones[k].box && line.cell == ones[k].cell;
        }
        if (!aside){
            pick = &ones[k];
            break;
        }
    }
    if (pick == nullptr){
        console.log(
            label + ": " + (ones.empty()
                ? "no " + upper(give) + " in the boxes (as they were read)"
                : "the only " + upper(give) + " of the boxes is the one to evolve") +
            ": nothing is taken out.",
            COLOR_ORANGE
        );
        return false;
    }
    //  The one that makes room: the party's sixth, when it is not of the team.
    std::string extra;
    const int extra_level = story_read_slot(rt, 6, &extra);
    if (extra_level < 0 || extra.empty()){
        console.log(
            label + ": party slot 6 has no Pokemon (or it was not read): with fewer than six in the party, "
            "nothing is taken out of a box.",
            COLOR_ORANGE
        );
        return false;
    }
    for (const StoryTeamMember& member : rt.team){
        if (member.line.count(extra) != 0){
            console.log(
                label + ": party slot 6 is " + upper(extra) + ", one of the team: it is not put in a box.",
                COLOR_ORANGE
            );
            return false;
        }
    }
    const int box = pick->box;
    const int cell = pick->cell;
    console.log(
        label + ": " + upper(give) + " (" + cell_text(box, cell) + ") comes into the party; " + upper(extra) +
        " waits in its cell (experimental).",
        COLOR_BLUE
    );
    rt.out_watch = trade_get;
    try{
        box_swap(rt, label, box, cell, give, extra);
    }catch (...){
        rt.out_watch.clear();
        throw;
    }
    rt.out_watch.clear();
    if (rt.out_made_before){
        //  The cell held what the trade gives: the list follows the game,
        //  and the POKeDEX table has it (the game's own count of the
        //  Pokedex has it from before: not counted again as new).
        rt.boxes.put(box, cell, trade_get, rt.out_made_level);
        rt.caught.insert(trade_get);
        if (rt.collection != nullptr){
            rt.collection->add_caught(trade_get, false, false);
        }
        console.log(
            "Got for the POKeDEX: " + trade_get + " (it lies in " + cell_text(box, cell) + " already: the trade was "
            "made before)." +
            (rt.collection != nullptr
                ? " The POKeDEX table: " + std::to_string(rt.collection->caught(trade_get)) + " of " +
                    std::to_string(std::max(0, rt.collection->needed(trade_get))) + "."
                : std::string()),
            COLOR_BLUE
        );
        story_note(rt, "Got " + trade_get, COLOR_GREEN);
        return false;
    }
    rt.out_box = box;
    rt.out_cell = cell;
    rt.out_extra = extra;
    rt.out_give = give;
    rt.out_now = give;
    //  The list follows the game (and goes back with it on a reset).
    rt.boxes.put(box, cell, extra, extra_level);
    return true;
}

void story_box_in(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    if (rt.out_box < 0 || rt.out_now.empty() || rt.out_extra.empty()){
        console.log(label + ": nothing was taken out of a box: nothing to put back.", COLOR_ORANGE);
        return;
    }
    const int level = box_swap(rt, label, rt.out_box, rt.out_cell, rt.out_extra, rt.out_now);
    rt.boxes.put(rt.out_box, rt.out_cell, rt.out_now, level);
    console.log(
        label + ": " + upper(rt.out_now) + " lies in " + cell_text(rt.out_box, rt.out_cell) + "; " +
        upper(rt.out_extra) + " is back in the party.",
        COLOR_BLUE
    );
    rt.out_box = -1;
    rt.out_cell = -1;
    rt.out_extra.clear();
    rt.out_give.clear();
    rt.out_now.clear();
}


//  ---- Room for an egg (build 47, experimental) ------------------------------

void story_egg_room(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    rt.egg_box = -1;
    rt.egg_cell = -1;
    rt.egg_extra.clear();
    rt.egg_party = 0;
    rt.egg_hatched.clear();
    const int count = story_party_count(rt);
    if (count < 6){
        rt.egg_party = count;
        console.log(
            label + ": the party has " + std::to_string(count) + " Pokemon: there is room for the egg. Nothing is "
            "put into a box.",
            COLOR_BLUE
        );
        return;
    }
    std::string extra;
    const int extra_level = story_read_slot(rt, 6, &extra);
    if (extra_level < 0 || extra.empty()){
        fail(rt, label + ": party slot 6 was not read: it is not known what to put into a box for the egg.");
    }
    for (const StoryTeamMember& member : rt.team){
        if (member.line.count(extra) != 0){
            fail(
                rt,
                label + ": party slot 6 is " + upper(extra) + ", one of the team: it is not put into a box, and the "
                "egg has no room. Take one Pokemon that is not of the team into the party and start again."
            );
        }
    }
    //  Empty cells by the list, from the last box back: what is caught fills
    //  the first boxes.
    std::vector<std::pair<int, int>> candidates;
    for (int box = BOX_COUNT - 1; box >= 0 && candidates.size() < 4; box--){
        for (int cell = BOX_ROWS * BOX_COLUMNS - 1; cell >= 0 && candidates.size() < 4; cell--){
            if (rt.boxes.at(box, cell) == nullptr){
                candidates.emplace_back(box, cell);
            }
        }
    }
    if (candidates.empty()){
        fail(rt, label + ": the boxes have no empty cell (as the list has them): the egg has no room.");
    }
    console.log(
        label + ": the party has six Pokemon; " + upper(extra) + " (level " + std::to_string(extra_level) +
        ") goes into an empty cell of the boxes for the egg (experimental).",
        COLOR_BLUE
    );
    int box = -1;
    int cell = -1;
    int level = -1;
    {
        StoryMemoryScope memory_scope("PC boxes");
        enter_box_view(rt, label);
        View view;
        to_box_name(rt, label);
        view.home = still_look(rt);
        level = store_last(rt, label, view, candidates, extra, box, cell);
        back_home(rt, label, view);
    }
    rt.boxes.put(box, cell, extra, level > 0 ? level : extra_level);
    rt.egg_box = box;
    rt.egg_cell = cell;
    rt.egg_extra = extra;
    rt.egg_party = 5;
}

void story_egg_back(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    if (rt.egg_box < 0 || rt.egg_extra.empty()){
        console.log(
            label + ": nothing was put into a box for the egg: what hatched stays in the party.",
            COLOR_BLUE
        );
        return;
    }
    std::string now;
    const int now_level = story_read_slot(rt, 6, &now);
    if (now_level < 0 || now.empty()){
        fail(rt, label + ": party slot 6 was not read: it is not known what goes into the box.");
    }
    if (!rt.egg_hatched.empty() && now != rt.egg_hatched){
        console.log(
            label + ": party slot 6 is " + upper(now) + ", not " + upper(rt.egg_hatched) + " as the hatching showed.",
            COLOR_ORANGE
        );
    }
    const int level = box_swap(rt, label, rt.egg_box, rt.egg_cell, rt.egg_extra, now);
    rt.boxes.put(rt.egg_box, rt.egg_cell, now, level);
    console.log(
        label + ": " + upper(now) + " lies in " + cell_text(rt.egg_box, rt.egg_cell) + "; " + upper(rt.egg_extra) +
        " is back in the party.",
        COLOR_BLUE
    );
    rt.egg_box = -1;
    rt.egg_cell = -1;
    rt.egg_extra.clear();
    rt.egg_party = 0;
    rt.egg_hatched.clear();
}


//  ---- The Day Care (build 48, experimental) ---------------------------------

namespace{

//  The Pokemon of the baby's evolution line that can have it with a DITTO
//  (the egg's species is the one at the root of the mother's line, whatever
//  the gender: daycare.c, GetEggSpecies; all of them are in the egg groups
//  that breed).
std::vector<std::string> daycare_family(const std::string& baby){
    if (baby == "pichu"){
        return {"pikachu", "raichu"};
    }
    if (baby == "cleffa"){
        return {"clefairy", "clefable"};
    }
    if (baby == "igglybuff"){
        return {"jigglypuff", "wigglytuff"};
    }
    if (baby == "togepi"){
        return {"togetic"};
    }
    if (baby == "smoochum"){
        return {"jynx"};
    }
    if (baby == "elekid"){
        return {"electabuzz"};
    }
    if (baby == "tyrogue"){
        return {"hitmonlee", "hitmonchan", "hitmontop"};
    }
    //  Build 54 (experimental): WYNAUT comes of a WOBBUFFET (and a DITTO) that
    //  holds the LAX INCENSE (daycare.c, AlterEggSpeciesWithIncenseItem: the
    //  egg is a WOBBUFFET without it).
    if (baby == "wynaut"){
        return {"wobbuffet"};
    }
    //  Build 54 (experimental): the second KABUTO (its parent: the one that is
    //  there, or the KABUTOPS it became), and EEVEE (the line of the story's
    //  team member: VAPOREON, JOLTEON or FLAREON; EEVEE itself too, once the
    //  Day Care made one).
    if (baby == "kabuto"){
        return {"kabuto", "kabutops"};
    }
    if (baby == "eevee"){
        return {"eevee", "vaporeon", "jolteon", "flareon"};
    }
    return {};
}

}

bool story_daycare_out(StoryRuntime& rt, const std::string& label, const std::string& baby){
    ConsoleHandle& console = rt.console;
    rt.dc_baby = baby;
    rt.dc_parents[0].clear();
    rt.dc_parents[1].clear();
    rt.dc_party = 0;
    rt.dc_given = false;
    rt.dc_egg = false;
    rt.dc_team_slot = 0;
    const std::vector<std::string> family = daycare_family(baby);
    if (family.empty()){
        fail(rt, label + ": " + upper(baby) + " has no parents in the program's list of the Day Care.");
    }
    auto is_family = [&](const std::string& slug){
        return std::find(family.begin(), family.end(), slug) != family.end();
    };
    auto skip = [&](const std::string& why){
        console.log(
            label + ": " + why + ": no " + upper(baby) + " is bred in this round (nothing was changed in the party "
            "or the boxes).",
            COLOR_ORANGE
        );
        return false;
    };
    //  EEVEE (build 54, experimental): the only one of its line is the team's
    //  (the gift became VAPOREON, JOLTEON or FLAREON): it is the parent, from
    //  its place in the party, with a DITTO of the boxes.
    int team_slot = 0;
    if (baby == "eevee"){
        for (size_t k = 0; k < rt.team.size() && k < 4 && team_slot == 0; k++){
            for (const std::string& name : family){
                if (rt.team[k].line.count(name) != 0){
                    team_slot = (int)k + 1;
                }
            }
        }
        if (team_slot == 0){
            return skip("no member of the team is of the line of EEVEE (the gift became one of them)");
        }
    }
    const int count = story_party_count(rt);
    if (count != 6){
        return skip(
            "the party has " + std::to_string(count) + " Pokemon, not six (the two parents take the party's fifth "
            "and sixth place, the ones that are there go into the boxes)"
        );
    }
    std::string slot[7];
    int level[7] = {-1, -1, -1, -1, -1, -1, -1};
    for (int k = 5; k <= 6; k++){
        level[k] = story_read_slot(rt, k, &slot[k]);
        if (level[k] < 0 || slot[k].empty()){
            return skip("party slot " + std::to_string(k) + " was not read");
        }
        for (const StoryTeamMember& member : rt.team){
            if (member.line.count(slot[k]) != 0){
                return skip("party slot " + std::to_string(k) + " is " + upper(slot[k]) + ", one of the team");
            }
        }
    }
    //  What the party's fifth and sixth place hold of the parents already
    //  (the parents of the segment before are still there).
    bool keep[7] = {false, false, false, false, false, false, false};
    bool have_ditto = false;
    bool have_parent = team_slot > 0;       //  (the team's: changed with the other place below)
    for (int k = 5; k <= 6; k++){
        if (!have_ditto && slot[k] == "ditto"){
            keep[k] = true;
            have_ditto = true;
        }else if (!have_parent && slot[k] != "ditto" && is_family(slot[k])){
            keep[k] = true;
            have_parent = true;
        }
    }
    //  What the boxes have for the rest: not the one that is set aside to
    //  evolve into a species the table still wants, and the lowest level.
    const std::vector<EvolveLine> plan = evolve_plan(story_evolve_table(rt), rt.boxes);
    //  allow_aside (build 54, KABUTO): the one KABUTO of the boxes is the one that
    //  is set aside for KABUTOPS, and the only parent there is: the egg is
    //  made first, and the plan sees the two KABUTO after it.
    auto pick_of = [&](const std::vector<std::string>& species, BoxPokemon& pick, bool allow_aside){
        bool found = false;
        for (int pass = 0; pass < 2 && !found; pass++){
            if (pass == 1 && !allow_aside){
                break;
            }
            for (const std::string& name : species){
                for (const BoxPokemon& one : rt.boxes.of(name)){
                    bool aside = false;
                    for (const EvolveLine& line : plan){
                        aside |= line.box == one.box && line.cell == one.cell;
                    }
                    if (aside && pass == 0){
                        continue;
                    }
                    if (!found || (one.level >= 0 && (pick.level < 0 || one.level < pick.level))){
                        pick = one;
                        found = true;
                    }
                }
            }
        }
        return found;
    };
    BoxPokemon ditto_pick;
    BoxPokemon parent_pick;
    if (!have_ditto && !pick_of({"ditto"}, ditto_pick, false)){
        return skip("no DITTO in the boxes (as they were read)");
    }
    if (!have_parent && !pick_of(family, parent_pick, baby == "kabuto")){
        std::string names;
        for (const std::string& name : family){
            names += (names.empty() ? "" : " or ") + upper(name);
        }
        return skip("no " + names + " in the boxes (as they were read)");
    }
    if (team_slot == 0 && have_ditto && have_parent){
        console.log(
            label + ": the party's fifth and sixth place hold " + upper(slot[5]) + " and " + upper(slot[6]) +
            " already: the parents.",
            COLOR_BLUE
        );
    }
    std::vector<BoxPokemon> picks;
    if (!have_ditto){
        picks.push_back(ditto_pick);
    }
    if (!have_parent){
        picks.push_back(parent_pick);
    }
    for (const BoxPokemon& pick : picks){
        //  The place that is not kept; the sixth, as a box takes the one
        //  that is there (the fifth changes places with it first).
        if (keep[6]){
            story_swap_party(rt, 5, 6);
            std::swap(slot[5], slot[6]);
            std::swap(level[5], level[6]);
            std::swap(keep[5], keep[6]);
        }
        console.log(
            label + ": " + upper(pick.slug) + " (" + cell_text(pick.box, pick.cell) + ") comes into the party; " +
            upper(slot[6]) + " waits in its cell (experimental).",
            COLOR_BLUE
        );
        box_swap(rt, label, pick.box, pick.cell, pick.slug, slot[6]);
        //  The list follows the game.
        rt.boxes.put(pick.box, pick.cell, slot[6], level[6]);
        slot[6] = pick.slug;
        level[6] = pick.level;
        keep[6] = true;
    }
    if (team_slot > 0){
        //  The place of the one that is not the DITTO changes with the team's
        //  member: both parents are the party's fifth and sixth then (the
        //  woman is given those two).
        const int other = slot[5] == "ditto" ? 6 : 5;
        std::string name;
        const int team_level = story_read_slot(rt, team_slot, &name);
        if (team_level < 0 || name.empty() || !is_family(name)){
            return skip(
                "party slot " + std::to_string(team_slot) + " is " + (name.empty() ? std::string("not read") : upper(name)) +
                ", not a Pokemon of the line of " + upper(baby)
            );
        }
        console.log(
            label + ": " + upper(name) + " (party slot " + std::to_string(team_slot) + ", the team's) is the "
            "parent of " + upper(baby) + ": it changes places with " + upper(slot[other]) + " (party slot " +
            std::to_string(other) + "; experimental).",
            COLOR_BLUE
        );
        story_swap_party(rt, team_slot, other);
        slot[other] = name;
        level[other] = team_level;
        rt.dc_team_slot = team_slot;
    }
    rt.dc_parents[0] = slot[5];
    rt.dc_parents[1] = slot[6];
    console.log(
        label + ": " + upper(slot[5]) + " and " + upper(slot[6]) + " are the party's fifth and sixth Pokemon: the "
        "parents of " + upper(baby) + ".",
        COLOR_BLUE
    );
    return true;
}

void story_daycare_store(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    if (rt.dc_party <= 0){
        fail(rt, label + ": the Day Care step before this one did not run: the size of the party is not known.");
    }
    const int count = story_party_count(rt);
    if (count != rt.dc_party + 1){
        fail(
            rt,
            label + ": the party has " + std::to_string(count) + " Pokemon, but " + std::to_string(rt.dc_party + 1) +
            " were expected (the team and what hatched). The game is reset."
        );
    }
    std::string baby;
    const int baby_level = story_read_slot(rt, count, &baby);
    if (baby_level < 0 || baby.empty()){
        fail(rt, label + ": party slot " + std::to_string(count) + " was not read: it is not known what hatched.");
    }
    if (baby != rt.dc_baby){
        console.log(
            label + ": party slot " + std::to_string(count) + " is " + upper(baby) + ", not " + upper(rt.dc_baby) +
            " as expected.",
            COLOR_ORANGE
        );
    }
    //  Empty cells by the list, from the last box back (as story_egg_room()).
    std::vector<std::pair<int, int>> candidates;
    for (int box = BOX_COUNT - 1; box >= 0 && candidates.size() < 4; box--){
        for (int cell = BOX_ROWS * BOX_COLUMNS - 1; cell >= 0 && candidates.size() < 4; cell--){
            if (rt.boxes.at(box, cell) == nullptr){
                candidates.emplace_back(box, cell);
            }
        }
    }
    if (candidates.empty()){
        fail(rt, label + ": the boxes have no empty cell (as the list has them): the baby has no room.");
    }
    console.log(
        label + ": " + upper(baby) + " (level " + std::to_string(baby_level) + ") goes into an empty cell of the "
        "boxes (experimental).",
        COLOR_BLUE
    );
    int box = -1;
    int cell = -1;
    int level = -1;
    {
        StoryMemoryScope memory_scope("PC boxes");
        enter_box_view(rt, label);
        View view;
        to_box_name(rt, label);
        view.home = still_look(rt);
        level = store_last(rt, label, view, candidates, baby, box, cell, count - 1);
        back_home(rt, label, view);
    }
    rt.boxes.put(box, cell, baby, level > 0 ? level : baby_level);
    console.log(
        label + ": " + upper(baby) + " lies in " + cell_text(box, cell) + "; the party has the team now.",
        COLOR_BLUE
    );
}


//  ---- Evolving by level (build 43) -------------------------------------------

std::vector<AutoStoryBoxes::LevelTarget> story_level_targets(StoryRuntime& rt){
    const std::vector<EvolveLine> plan = evolve_plan(story_evolve_table(rt), rt.boxes);
    //  One that failed twice in a row in this run is left for the next run.
    std::set<std::string> without;
    std::set<std::string> exp_without;
    for (const EvolveLine& line : plan){
        if (rt.places.place_out(story_level_place(line.slug))){
            without.insert(line.slug);
        }
        if (rt.places.place_out(story_exp_place(line.slug))){
            exp_without.insert(line.slug);
        }
    }
    //  Build 56: the ones with EXP_ROUND_MIN points to go or more are left to
    //  the rounds of the Elite Four (there are rounds after these segments
    //  too: a DRATINI that evolves here is a DRAGONAIR with 170000 points to
    //  go, eight hours at Victory Road, six rounds of the League). One that
    //  failed in the rounds twice in a row is not left to them: it is trained
    //  here, slowly, as it was before.
    return level_targets_light(plan, without, exp_without);
}

std::vector<AutoStoryBoxes::LevelTarget> story_friend_targets(StoryRuntime& rt){
    const std::vector<EvolveLine> plan = evolve_plan(story_evolve_table(rt), rt.boxes);
    //  One that failed twice in a row in this run is left for the next run.
    std::set<std::string> without;
    for (const EvolveLine& line : plan){
        if (rt.places.place_out(story_friend_place(line.slug))){
            without.insert(line.slug);
        }
    }
    return friend_targets(plan, rt.friend_records, without);
}

std::vector<AutoStoryBoxes::LevelTarget> story_exp_targets(StoryRuntime& rt){
    const std::vector<EvolveLine> plan = evolve_plan(story_evolve_table(rt), rt.boxes);
    //  One that failed twice in a row in this run is left for the next run.
    std::set<std::string> without;
    for (const EvolveLine& line : plan){
        if (rt.places.place_out(story_exp_place(line.slug))){
            without.insert(line.slug);
        }
    }
    return exp_targets(plan, without);
}

bool story_level_out(
    StoryRuntime& rt, const std::string& label, LevelMode mode, const std::function<void()>& changed
){
    ConsoleHandle& console = rt.console;
    const bool friendship = mode == LevelMode::FRIENDSHIP;
    const bool by_exp = mode == LevelMode::EXP;
    rt.out_box = -1;
    rt.out_cell = -1;
    rt.out_extra.clear();
    rt.out_give.clear();
    rt.out_now.clear();
    rt.out_becomes.clear();
    rt.out_goal = 0;
    rt.out_friend = false;
    const std::vector<LevelTarget> targets =
        by_exp ? story_exp_targets(rt) : friendship ? story_friend_targets(rt) : story_level_targets(rt);
    if (targets.empty()){
        console.log(
            label + ": nothing in the boxes is left to evolve by " + (friendship ? "friendship" : "a level") +
            (by_exp ? " that is worth a round of the Elite Four" : "") + ": nothing is taken out.",
            COLOR_BLUE
        );
        return false;
    }
    const LevelTarget target = targets.front();
    if (by_exp){
        console.log(
            label + ": " + std::to_string(targets.size()) + " of the boxes can evolve by a level and have " +
            std::to_string(EXP_ROUND_MIN) + " experience points to go or more (the most first): " +
            exp_targets_text(targets) + ".",
            COLOR_BLUE
        );
    }else if (friendship){
        console.log(
            label + ": " + std::to_string(targets.size()) + " of the boxes can evolve by friendship (the fewest blocks "
            "of steps to walk first): " + friend_targets_text(targets) + ".",
            COLOR_BLUE
        );
    }else{
        console.log(
            label + ": " + std::to_string(targets.size()) + " of the boxes can evolve by a level (the fewest levels "
            "to go first): " + level_targets_text(targets) + ".",
            COLOR_BLUE
        );
    }
    //  The one that makes room: the party's sixth, when it is not of the team.
    std::string extra;
    const int extra_level = story_read_slot(rt, 6, &extra);
    if (extra_level < 0 || extra.empty()){
        console.log(
            label + ": party slot 6 has no Pokemon (or it was not read): with fewer than six in the party, "
            "nothing is taken out of a box.",
            COLOR_ORANGE
        );
        return false;
    }
    for (const StoryTeamMember& member : rt.team){
        if (member.line.count(extra) != 0){
            console.log(
                label + ": party slot 6 is " + upper(extra) + ", one of the team: it is not put in a box.",
                COLOR_ORANGE
            );
            return false;
        }
    }
    const std::string from = upper(target.from_slug);
    story_note(rt, label + ": " + target.name);
    console.log(
        label + ": " + from + " (" + cell_text(target.box, target.cell) + (target.level > 0 ? ", level " +
        std::to_string(target.level) : std::string()) + ") comes into the party, " +
        (by_exp ? "to ride through the Elite Four with the EXP. SHARE (about " + std::to_string(target.exp) +
            " experience points to go) up to level " :
         friendship ? "to walk " + std::to_string(std::max(0, target.blocks - target.walked)) + " more blocks of " +
            std::to_string(FRIEND_BLOCK_STEPS) + " steps for its friendship and then be trained to level " :
            std::string("to be trained to level ")) +
        std::to_string(target.goal) + " for " + target.name + (from == "TYROGUE"
            ? " (or HITMONLEE or HITMONTOP: its Attack against its Defense at level " +
                std::to_string(TYROGUE_EVOLVES_AT) + " decides)"
            : std::string()) +
        "; " + upper(extra) + " waits in its cell (experimental).",
        COLOR_BLUE
    );
    box_swap(rt, label, target.box, target.cell, target.from_slug, extra);
    //  What is there now, and the level it has really.
    std::string now;
    const int level = story_read_slot(rt, 6, &now);
    if (now != target.from_slug){
        fail(
            rt,
            label + ": party slot 6 is " + (now.empty() ? std::string("not read") : upper(now)) + " after the change, "
            "not " + from + ". Nothing is saved: the game is reset."
        );
    }
    int goal = target.goal;
    if (level > 0){
        goal = friendship ? (level >= 100 ? 0 : level + 1) : level_goal(level, target.evolves_at);
    }
    rt.out_friend = friendship;
    rt.out_box = target.box;
    rt.out_cell = target.cell;
    rt.out_extra = extra;
    rt.out_give = target.from_slug;
    rt.out_now = target.from_slug;
    rt.out_becomes = target.slug;
    rt.out_goal = goal;
    //  The list follows the game (and goes back with it on a reset).
    rt.boxes.put(target.box, target.cell, extra, extra_level);
    //  The Elite Four for experience: the list had an older level of it, and
    //  what it has to go is not worth a round (the list has the real level
    //  from now on, so the next round takes another one).
    const bool too_little = by_exp && goal > 0 && level > 0 && exp_to_go(level, goal) < EXP_ROUND_MIN;
    if (goal <= 0 || too_little){
        //  (Level 100: it cannot go up.) Back where it was.
        console.log(
            label + ": " + from + " is at level " + std::to_string(level) + ": " +
            (too_little
                ? "only about " + std::to_string(exp_to_go(level, goal)) + " experience points to go for level " +
                    std::to_string(goal) + ", too little for a round of the Elite Four (Victory Road is as quick)."
                : std::string("it cannot be trained.")),
            COLOR_ORANGE
        );
        box_swap(rt, label, target.box, target.cell, extra, target.from_slug);
        rt.boxes.put(target.box, target.cell, target.from_slug, level);
        rt.out_box = -1;
        rt.out_cell = -1;
        rt.out_extra.clear();
        rt.out_give.clear();
        rt.out_now.clear();
        rt.out_becomes.clear();
        rt.out_friend = false;
        return false;
    }
    if (changed){
        changed();
    }
    return true;
}

void story_level_in(StoryRuntime& rt, const std::string& label, const std::function<void()>& changed){
    ConsoleHandle& console = rt.console;
    if (rt.out_box < 0 || rt.out_extra.empty() || rt.out_give.empty()){
        console.log(label + ": nothing was taken out of a box for a training: nothing to put back.", COLOR_ORANGE);
        return;
    }
    std::string now;
    const int level = story_read_slot(rt, 6, &now);
    //  (TYROGUE: any of its three evolutions.)
    const std::set<std::string> line = training_line(rt.out_give, rt.out_becomes);
    if (now.empty() || line.count(now) == 0){
        fail(
            rt,
            label + ": party slot 6 is " + (now.empty() ? std::string("not read") : upper(now)) + ", not " +
            upper(rt.out_give) + " or " + upper(rt.out_becomes) + ". Nothing is saved: the game is reset."
        );
    }
    const std::string where = cell_text(rt.out_box, rt.out_cell);
    const std::string waiting = rt.out_extra;
    const std::string was = rt.out_give;
    const std::string becomes = rt.out_becomes;
    const int goal = rt.out_goal;
    const bool friendship = rt.out_friend;
    box_swap(rt, label, rt.out_box, rt.out_cell, rt.out_extra, now);
    if (rt.boxes_known){
        rt.boxes.put(rt.out_box, rt.out_cell, now, level);
    }
    rt.out_box = -1;
    rt.out_cell = -1;
    rt.out_extra.clear();
    rt.out_give.clear();
    rt.out_now.clear();
    rt.out_becomes.clear();
    rt.out_goal = 0;
    rt.out_friend = false;
    //  What was walked with it (evolving by friendship): done with when it
    //  evolved, one more level trained without it when it did not (the next
    //  segment walks a wider margin).
    int walked = 0;
    int tests = 0;
    if (friendship){
        const auto found = rt.friend_records.find(was);
        if (found != rt.friend_records.end()){
            walked = found->second.blocks;
            tests = found->second.tests;
        }
        if (now == becomes && now != was){
            rt.friend_records.erase(was);
        }else{
            rt.friend_records[was].tests = tests + 1;
        }
    }
    if (line.count(now) != 0 && now != was){
        //  One more in the POKeDEX table, the game saved.
        rt.caught.insert(now);
        if (rt.collection != nullptr){
            rt.collection->add_caught(now, false, false);
        }
        rt.dex_new_since_load.insert(now);
        std::string table;
        if (rt.collection != nullptr){
            size_t registered = 0;
            size_t total = 0;
            size_t to_catch = 0;
            rt.collection->counts_summary(registered, total, to_catch);
            table = " The POKeDEX table: " + std::to_string(registered) + " of " + std::to_string(total) +
                " species with at least one.";
        }
        if (now != becomes){
            //  TYROGUE: the game chose, by its Attack and Defense at level 20.
            console.log(
                "Evolved into " + upper(now) + ", not into the " + upper(becomes) + " that the plan had for it: the "
                "game decides between HITMONLEE (Attack above Defense), HITMONCHAN (below) and HITMONTOP (equal).",
                COLOR_ORANGE
            );
        }
        console.log(
            "Evolved for the POKeDEX: " + now + " (" + upper(was) +
            (friendship ? " by friendship, after " + std::to_string(walked) + " blocks of " +
                std::to_string(FRIEND_BLOCK_STEPS) + " steps walked, at level " : std::string(" by level ")) +
            (level > 0 ? std::to_string(level) : std::string("?")) + "). It lies in " + where + "; " + upper(waiting) +
            " is back in the party." +
            table,
            COLOR_BLUE
        );
        story_note(rt, "Evolved: " + now, COLOR_GREEN);
    }else{
        console.log(
            label + ": " + upper(now) + " is at level " + (level > 0 ? std::to_string(level) : std::string("?")) +
            ", the goal was " + std::to_string(goal) + ": it did not evolve. It lies in " + where + "; " +
            upper(waiting) + " is back in the party. " +
            (friendship ? "Its friendship was not 220 yet (" + std::to_string(walked) + " blocks of " +
                std::to_string(FRIEND_BLOCK_STEPS) + " steps walked, " + std::to_string(tests + 1) +
                " levels trained without it): the next segment walks more. " : std::string()) +
            "The next segment goes on with the list as it is now.",
            COLOR_ORANGE
        );
    }
    story_save(rt, true);
    story_saved(rt, rt.step_index + 1);
    if (changed){
        changed();
    }
}


}
}
}

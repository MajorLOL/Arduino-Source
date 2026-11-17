/*  Pokemon FRLG Autostory Box Scan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Experimental (PocketPilot build 34: not run on a console when written).
 *  StepType::BOXES: the player faces the PC of a Pokemon Center. Into
 *  BILL's PC and MOVE POKeMON, every cell of every box looked at, out
 *  again. What the rules of it are, and why nothing in the boxes can change:
 *  PokemonFRLG_AutoStoryBoxes.h.
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
#include <optional>
#include <string>
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
void to_box_name(StoryRuntime& rt, const std::string& label){
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 5; attempt++){
        const std::optional<BoxSelection> hand = find_hand(rt, 4);
        if (!hand){
            fail(rt, label + ": the hand of the box view is not seen (on the way to the box's name).");
        }
        if (hand->closed){
            //  Not the program's doing (it presses no A here): nothing more
            //  is pressed.
            fail(rt, label + ": the hand of the box view holds a Pokemon. Put it down by hand; the boxes are not read like this.");
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
Cell into_cells(StoryRuntime& rt, const std::string& label){
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
            to_box_name(rt, label);
        }
    }
    fail(rt, label + ": the hand of the box view did not get from the box's name into the cells.");
}

//  The hand from the cell it is on to another one, seen there. The moves
//  are made blind and then looked at, as the Box Sorter does. A hand that
//  is not seen at all (rare: it moves a little, and what is behind it
//  matters) is taken to be where the moves put it; "unseen" counts those.
Cell move_to(StoryRuntime& rt, const std::string& label, Cell at, const Cell& target, bool at_seen, int& unseen){
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
        if (hand->closed){
            fail(rt, label + ": the hand of the box view holds a Pokemon. Put it down by hand; the boxes are not read like this.");
        }
        if (hand->location != BoxSelectionLocation::Grid){
            //  It left the cells (a press too many at an edge): back in by
            //  the box's name.
            to_box_name(rt, label);
            at = into_cells(rt, label);
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

//  On the box's name: RIGHT, the next box (the fourteenth: the first again).
//  Returns the look of it once it stands still.
std::vector<uint8_t> next_box(StoryRuntime& rt, const std::string& label, const std::vector<uint8_t>& before){
    ProControllerContext& context = rt.context;
    //  (Held, not pressed: HandleInput_BoxTitle looks at the held keys. The
    //  slide takes 32 frames, and nothing is read from the D-pad during it.)
    pbf_press_dpad(context, DPAD_RIGHT, 100ms, 0ms);
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
            label + ": the box did not look any different after RIGHT on its name (two boxes alike, or the press "
            "was not taken).",
            COLOR_ORANGE
        );
    }
    return after;
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
        std::string names;
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
            names += (names.empty() ? "" : ", ") +
                (one.slug.empty() ? std::string("(not read)") : upper(one.slug)) +
                (one.level > 0 ? " " + std::to_string(one.level) : std::string());
        }
        census.boxes = box + 1;
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


}
}
}

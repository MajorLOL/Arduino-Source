/*  PocketPilot Claude Plays: any game
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The tools for a game the program has no routines for: Claude looks at
 *  the screen, has text read from it, and works the controller.
 *
 *  -   "look" sends a picture of the screen or of a part of it, "read_text"
 *      the text the program reads there (the OCR of the other programs):
 *      text costs a fraction of a picture's tokens, and reads better than
 *      small print in a small picture.
 *
 *  -   "press", "hold" and "mash" work the controller, "wait" waits for a
 *      time, until the picture is still, or until it changes.
 *
 *  -   Every result ends with what the program sees without help: how much
 *      of the picture changed through the action, and whether it still
 *      moves. That tells whether a press did anything, for a few tokens.
 *
 *  -   Where the game's other programs have a routine to save the game, and
 *      the user allows saving, it is a tool too.
 *
 *  The console is behind an interface (PlayConsole), so that all of this
 *  runs in a test with a made-up console.
 *
 *  No Qt in this header: programs of the games include it.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ClaudePlays_AnyGame_H
#define PokemonAutomation_PocketPilot_ClaudePlays_AnyGame_H

#include <stdint.h>
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "ClaudePlays_Engine.h"

namespace PokemonAutomation{

class ImageViewRGB32;

namespace PocketPilot{
namespace ClaudePlays{


//  The controller of a session, as Claude knows it.
struct PadLayout{
    //  For Claude: "a Nintendo Switch Pro Controller".
    std::string name;
    //  Its buttons by the names Claude uses, UP, DOWN, LEFT and RIGHT
    //  included. Without HOME.
    std::vector<std::string> buttons;
    //  What "hold" calls its sticks: "left_stick" and "right_stick", or one
    //  "stick".
    std::vector<std::string> sticks;
    //  UP, DOWN, LEFT and RIGHT are a short push of the (first) stick: the
    //  controller has no d-pad.
    bool directions_are_stick = false;

    bool has_button(const std::string& button) const;
};
const PadLayout& pro_controller_layout();
const PadLayout& right_joycon_layout();


//  What is held on the controller at one moment.
struct PadInput{
    //  Held together, by the names of PadLayout::buttons (or HOME). With a
    //  d-pad: at most two of its directions, next to each other.
    std::vector<std::string> buttons;
    //  -1 to 1: x to the right, y up. 0, 0: the stick is let go. "stick"
    //  is the first stick of the layout, "stick2" its second.
    double stick_x = 0;
    double stick_y = 0;
    double stick2_x = 0;
    double stick2_y = 0;
};


//  The console of a session.
class PlayConsole{
public:
    virtual ~PlayConsole() = default;

    virtual const PadLayout& layout() const = 0;

    //  Holds "input" for "duration", then lets go.
    virtual void hold(const PadInput& input, std::chrono::milliseconds duration) = 0;
    //  Presses one button again and again for "duration".
    virtual void mash(const std::string& button, std::chrono::milliseconds duration) = 0;
    //  Nothing held for "duration".
    virtual void wait(std::chrono::milliseconds duration) = 0;

    //  The picture on the screen once all of the above has been done on the
    //  console: "use" is called with it. False if no video comes in.
    virtual bool screen(const std::function<void(const ImageViewRGB32&)>& use) = 0;
    //  The text in a picture as the program reads it, its lines from top to
    //  bottom.
    virtual std::string read_text(const ImageViewRGB32& picture) = 0;

    //  The game's other programs have a routine that saves the game.
    virtual bool can_save() const{ return false; }
    //  Saves the game with it, and says what came of it.
    virtual PlayResult save_game(){ return PlayResult(); }
};


//  What Claude is told about the game.
struct GameFacts{
    //  "Pokemon Scarlet or Violet". Empty: the program does not know which
    //  game is on the screen.
    std::string name;
    //  Its controls and what else helps, as the program knows them.
    std::string notes;
    //  From the user, about the game or this session.
    std::string user_notes;
};

struct AnyGameOptions{
    //  How large a picture of the whole screen is. A part of the screen is
    //  shown at twice the detail.
    int picture_width = 640;
    int picture_height = 360;
    //  The user lets Claude save the game.
    bool allow_save = false;
    //  The user lets Claude press HOME.
    bool allow_home = false;
};

//  The tools and the instructions for a controller and these options.
//  "save_routine": the game has a routine to save (PlayConsole::can_save()).
std::vector<PlayTool> any_game_tools(
    const PadLayout& layout, const AnyGameOptions& options, bool save_routine
);
std::string any_game_instructions(
    const PadLayout& layout, const AnyGameOptions& options, const GameFacts& facts, bool save_routine
);

//  How many tokens a picture of this size costs.
int picture_tokens(int width, int height);


class AnyGame : public Game{
public:
    AnyGame(PlayConsole& console, GameFacts facts, AnyGameOptions options);

    virtual std::vector<PlayTool> tools() const override;
    virtual std::string instructions() const override;
    virtual std::string screen() override;
    virtual PlayResult run(const PlayCall& call) override;

private:
    PlayConsole& m_console;
    GameFacts m_facts;
    AnyGameOptions m_options;
};



//  The screen in small, to tell one picture from another: the average
//  color of each cell of a grid.
struct ScreenThumb{
    static const int WIDTH = 64;
    static const int HEIGHT = 36;
    //  Red, green, blue of each cell, row by row. Empty: no picture.
    std::vector<uint8_t> cells;

    bool valid() const{ return !cells.empty(); }
};
ScreenThumb screen_thumb(const ImageViewRGB32& picture);
//  The part of the cells that differ, 0 to 1. (The noise of a capture card
//  does not make a cell differ.) 1 if one of the two has no picture.
double thumb_difference(const ScreenThumb& first, const ScreenThumb& second);
//  A black screen: a fade, or a scene loading.
bool thumb_is_black(const ScreenThumb& thumb);



}
}
}
#endif

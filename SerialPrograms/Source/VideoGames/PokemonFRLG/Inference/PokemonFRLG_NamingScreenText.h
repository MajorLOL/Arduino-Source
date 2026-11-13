/*  Pokemon FRLG Naming Screen Text
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The naming screen's keyboard (its characters and where their keys are),
 *  the list of names a person writes for it, and a reader for the name being
 *  typed: every character of it is compared, pixel by pixel, with the game's
 *  own font. Nothing here needs the framework, so all of it is tested off
 *  the console, on pictures of the screen.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_NamingScreenText_H
#define PokemonAutomation_PokemonFRLG_NamingScreenText_H

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace NamingScreen{


//  The keyboard's pages, in the order SELECT goes through them
//  (naming_screen.c: UPPER, lower, others, and UPPER again).
const int PAGE_UPPER = 0;
const int PAGE_LOWER = 1;
const int PAGE_OTHERS = 2;
const int PAGES = 3;
const int ROWS = 4;

//  The most characters a name on this screen has (a box name).
const int MAX_CHARACTERS = 8;

//  The key columns of a page. The buttons (the page, BACK, OK) are one
//  column further.
int columns(int page);

//  What the key types. The empty keys type a space.
char32_t key(int page, int x, int y);

//  Whether the English keyboard has the character.
bool can_type(char32_t character);

//  The page to type the character on: "page" if it has it, else the first
//  one after it that does. -1 if no page has it.
int page_for(char32_t character, int page);

//  The key for the character on the page that takes the fewest presses from
//  (from_x, from_y). The rows wrap around; the columns are not taken around
//  (the buttons are in between). False if the page does not have it.
bool nearest_key(char32_t character, int page, int from_x, int from_y, int& x, int& y);

//  The key nearest to (from_x, from_y) on the page that types something to
//  see (not a space).
bool nearest_visible_key(int page, int from_x, int from_y, int& x, int& y);

//  A press on the naming screen (naming_screen.c: Input_Enabled).
enum class Press{
    A,          //  Types the key under the cursor (or presses the button).
    B,          //  Takes the last character away.
    SELECT,     //  The next page.
    START,      //  The cursor to OK.
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

//  Where the cursor is on the keys.
struct Keys{
    int page = PAGE_UPPER;
    int x = 0;
    int y = 0;
};

//  A step of typing a name: the presses, and after them the page the
//  keyboard is on and what the name's field shows.
struct TypingStep{
    std::vector<Press> presses;
    int page = PAGE_UPPER;
    std::u32string shows;
};

//  The steps that type "name" into an empty field of "max_characters"
//  places, with the cursor at "keys": one step a character.
//  A space in the last place cannot be told from an empty last place on the
//  screen, so it takes two steps: first a character that can be seen (the
//  name is full then, which puts the cursor on OK), then LEFT, which from
//  OK is the empty key at the end of the bottom row on every page, and A:
//  a full name takes every further character in its last place.
//  Empty if the name cannot be typed (a character the keyboard does not
//  have, too long).
std::vector<TypingStep> typing_steps(const std::u32string& name, int max_characters, Keys keys);

//  The presses that empty the field whatever is in it and put the cursor
//  where it is known, on the first key of the bottom row: B for every place
//  and once more, START (the cursor to OK), RIGHT (from OK: that key). The
//  page stays the one it is.
std::vector<Press> restart_presses(int max_characters);

std::string to_utf8(const std::u32string& text);

//  The name as the log shows it: between brackets, so that spaces at its
//  ends can be told ("[ FoHIoor]").
std::string in_brackets(const std::u32string& name);

//  A character for the log: itself, or what it is called.
std::string character_name(char32_t character);


//  A line of the list of names.
struct NameLine{
    int line = 0;           //  Its line in the text, from 1.
    int box = 0;            //  The box number written in front of it ("Box 3:"), 0 if none.
    std::u32string name;
};

//  The list of names, one per line, as a person writes or pastes it:
//
//      CCUnnREo                            the name itself
//      [ FoHIoor]                          between brackets: exactly this
//      Box  6: _ F o H I o o r [ FoHIoor]  the way ACE codes are published
//      Box 7: xn
//
//  An underscore stands for a space (the game's keyboard has no underscore).
//  With brackets, the name is what is between them. Empty lines, and lines
//  for "Box 8+", are left out. Returns false, with the reason in "error",
//  when a line cannot be typed as it stands: a character the keyboard does
//  not have, more than "max_characters", only spaces (the game does not
//  keep such a name), a line that can be read in two ways, or box numbers
//  that are not in rising order.
bool parse_name_lines(
    const std::string& utf8, int max_characters,
    std::vector<NameLine>& lines, std::string& error
);


//  The colour 0xRRGGBB of the screen at (x, y), in the game's own pixels
//  (240 by 160): the middle of the pixel in column 3 is x = 3.5.
using Pixel = std::function<uint32_t(double x, double y)>;

//  The game's pixels in a picture of exactly the game's screen (the game box
//  of the video): rows of 0xAARRGGBB. Each is the mean of a few picture
//  pixels around its middle. The picture must outlive what is returned.
Pixel game_pixels(const uint32_t* data, size_t bytes_per_row, size_t width, size_t height);

//  The page the keyboard is on, told by the colour of its panel (blue for
//  UPPER, orange for lower, green for the others); -1 if the picture is not
//  the naming screen, or shows it between two pages.
int keyboard_page(const Pixel& pixel);

//  The name being typed, as the screen shows it.
struct Entry{
    //  The field was found (the row of underscores under the name).
    bool found = false;
    int slots = 0;
    //  What each slot shows: U' ' for nothing, 0 for something that is not
    //  exactly one of the keyboard's characters.
    char32_t characters[MAX_CHARACTERS] = {};
    //  ... and for those, the character that comes closest and in how many
    //  pixels it differs.
    char32_t closest[MAX_CHARACTERS] = {};
    int differences[MAX_CHARACTERS] = {};
    //  The slot the next character goes to (its underscore sits lower than
    //  the others): the number of characters typed, or the last slot when
    //  the name is full. -1 if the underscores do not tell.
    int cursor = -1;
    //  Where the field is against where it should be, in game pixels.
    double dx = 0;
    double dy = 0;

    //  For the log: "[CCU?    ] next: 4".
    std::string summary() const;
};

Entry read_entry(const Pixel& pixel, int max_characters);

//  Whether the screen shows exactly "typed" (the characters typed so far)
//  and nothing after it, with the next character due where it should be.
//  Returns what differs, or an empty string.
std::string entry_difference(const Entry& entry, const std::u32string& typed);

//  A look at the title of the box on the storage screen, to see whether it
//  shows "name".
struct TitleLook{
    //  The share of the name's white pixels that are white and of its shadow
    //  pixels that are dark, the lower of the two, where the name fits best
    //  within 2 pixels: 0 to 1. The name the title has: 0.97 or more.
    //  Another name: less, the more it differs (one other character of
    //  eight can still give 0.98).
    double match = 0;
    //  The first place where another character of the keyboard explains
    //  what the title shows clearly better than the name's own (-1: none),
    //  and that character.
    int other_at = -1;
    char32_t other = 0;

    bool shows_name() const{ return match >= 0.95 && other_at < 0; }
};
TitleLook look_at_title(const Pixel& pixel, const std::u32string& name);



}
}
}
}
#endif

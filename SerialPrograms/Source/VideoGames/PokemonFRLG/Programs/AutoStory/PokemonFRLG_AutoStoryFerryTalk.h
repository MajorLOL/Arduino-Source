/*  Pokemon FRLG Autostory Ferry Talk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The sailor at the pier of Vermilion City, pressed through without
 *  reading him.
 *
 *  What people say is blue or red in these games (his script sets
 *  "textcolor NPC_TEXT_COLOR_MALE"), and the program's text reading takes
 *  dark text only: his text box is seen, its text is not. On 15 November
 *  2026 his first page about the MYSTICTICKET stood there until the step
 *  gave up, three times from the same save.
 *
 *  His talk with the RAINBOW PASS and both tickets, as the Switch version
 *  has them after the Hall of Fame (VermilionCity_EventScript_CheckTicket,
 *  ..._HasMysticAndAuroraTickets, EventScript_SeviiDestinationsPage1,
 *  EventScript_DepartingVermilion):
 *    - the first time only: "Oh! That's a MYSTICTICKET! Now that is rare."
 *      (with the advance arrow), "We'll be happy to take you to NAVEL ROCK
 *      anytime." (without it: it waits for a button all the same), and the
 *      same two pages for the AURORATICKET;
 *    - "Would you like to board a SEAGALLOP ferry?" with a menu: SEVII
 *      ISLANDS, NAVEL ROCK, BIRTH ISLAND, EXIT;
 *    - the same text with the Sevii Islands list: ONE ISLAND ... FOUR
 *      ISLAND, OTHER, EXIT;
 *    - "Okay, everything's in order." (arrow), "SEAGALLOP HI-SPEED 7 will
 *      be departing immediately." (no arrow), and the text box closes for
 *      the ferry.
 *  The text box stays up from his first word to his last.
 *
 *  For the first row of both menus (ONE ISLAND), A is right on each of
 *  these. So A goes to every page: at once on one with the advance arrow,
 *  and at the second look on one without it (it may still be printing: A
 *  only hurries the text then, and the next look finds the page again).
 *  The talk is over when the text box is gone for a few looks. A departure
 *  takes four presses at the least (the two menus and the two pages after
 *  them): with fewer, the looks go on for longer first.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFerryTalk_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFerryTalk_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryFerryTalk{


//  What a look at the picture shows.
enum class Look{
    NO_BOX,     //  no text box
    BOX,        //  a text box without the advance arrow
    ARROW,      //  a text box with the advance arrow
};

enum class Act{
    WAIT,       //  look again in half a second
    PRESS_A,
    LEFT,       //  the text box is gone: the ferry leaves
    NO_TEXT,    //  no text box came up at all
};

const int FEWEST_PRESSES = 4;       //  a departure takes these at the least
const int LOOKS_AT_A_BOX = 2;       //  at a box without the arrow, before A
const int LOOKS_TO_LEAVE = 4;       //  without a text box, after FEWEST_PRESSES
const int LOOKS_TO_LEAVE_UNSURE = 12;   //  the same after fewer presses
const int LOOKS_FOR_TEXT = 20;      //  without a text box, nothing pressed yet

struct Talk{
    int quiet = 0;      //  looks in a row without a text box
    int box_looks = 0;  //  looks in a row at a text box without the arrow
    int pressed = 0;    //  A's so far (the caller adds its own on a menu)

    //  After LEFT: whether the talk was as long as a departure is.
    bool long_enough() const{
        return pressed >= FEWEST_PRESSES;
    }
};

//  "first_row": A is right on a menu too (the first row of the list is
//  wanted, or the row was picked already). Without it, only a page with
//  the advance arrow gets A.
inline Act next(Talk& talk, Look look, bool first_row){
    if (look != Look::NO_BOX){
        talk.quiet = 0;
        if (look == Look::ARROW || (first_row && ++talk.box_looks >= LOOKS_AT_A_BOX)){
            talk.box_looks = 0;
            talk.pressed++;
            return Act::PRESS_A;
        }
        return Act::WAIT;
    }
    talk.box_looks = 0;
    talk.quiet++;
    if (talk.pressed == 0){
        return talk.quiet >= LOOKS_FOR_TEXT ? Act::NO_TEXT : Act::WAIT;
    }
    const int looks = talk.long_enough() ? LOOKS_TO_LEAVE : LOOKS_TO_LEAVE_UNSURE;
    return talk.quiet >= looks ? Act::LEFT : Act::WAIT;
}


}
}
}
}
#endif

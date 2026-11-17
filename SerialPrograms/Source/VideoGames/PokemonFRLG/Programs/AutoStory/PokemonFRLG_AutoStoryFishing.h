/*  Pokemon FRLG Autostory Fishing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Experimental. Fishing for the POKeDEX collection (a DEX step with
 *  p2 & 4): the player stands on land facing water, and a rod is cast again
 *  and again.
 *
 *  A cast in FireRed and LeafGreen needs no button at the right moment
 *  (src/field_player_avatar.c, Task_Fishing): a second of waiting, 4 to 10
 *  dots in the text box (20 frames each), then one of two texts, each
 *  waiting for a button:
 *
 *      "Not even a nibble..."          half of the casts (Fishing6: Random() & 1)
 *      "A POKeMON's on the hook!"      the other half: a wild battle follows
 *
 *  Which wild Pokemon: the rod's own slots of the map the player stands on
 *  (OLD ROD 70/30, GOOD ROD 60/20/20, SUPER ROD 40/40/15/4/1).
 *
 *  Where no rod can be cast (the player does not face water that could be
 *  surfed onto from where they stand), the bag stays open with PROF. OAK's
 *  words: "... There's a time and place for everything! But not now."
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFishing_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFishing_H

#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryFishing{


//  The rods, as the story's data counts them (a fishing DEX step's p4;
//  SKIP_IF_ITEM and SKIP_IF_COLLECTED: the rod + 1).
const int ROD_COUNT = 3;

inline bool is_rod(int rod){
    return rod >= 0 && rod < ROD_COUNT;
}

inline const char* rod_name(int rod){
    switch (rod){
    case 0:     return "OLD ROD";
    case 1:     return "GOOD ROD";
    case 2:     return "SUPER ROD";
    default:    return "rod";
    }
}

//  Who gives it (for the messages).
inline const char* rod_giver(int rod){
    switch (rod){
    case 0:     return "the Fishing Guru in his house in Vermilion City";
    case 1:     return "the Fishing Guru's brother in his house in Fuchsia City";
    case 2:     return "the Fishing Guru's brother in the house on Route 12";
    default:    return "a Fishing Guru";
    }
}

//  The rod's description in the bag, as the program reads it (lower case):
//      OLD ROD     "An old and beat-up fishing rod. Use it by any body of water to fish for wild POKeMON."
//      GOOD ROD    "A new, good-quality fishing rod. Use it by any body ..."
//      SUPER ROD   "An awesome, high-tech fishing rod. Use it by any body ..."
//  One of "words" and none of "avoid" (the three differ in their first
//  words only; a letter or the hyphen read wrong must not make one rod pass
//  for another).
struct RodWords{
    std::vector<const char*> words;
    std::vector<const char*> avoid;
};

inline RodWords rod_words(int rod){
    switch (rod){
    case 0:
        return {{"beat-up", "beat up", "beatup", "old and beat"}, {"good-quality", "good quality", "high-tech", "high tech", "awesome"}};
    case 1:
        return {{"good-quality", "good quality", "goodquality", "new, good", "new good"}, {"beat-up", "beat up", "high-tech", "high tech", "awesome"}};
    case 2:
        return {{"high-tech", "high tech", "hightech", "awesome"}, {"beat-up", "beat up", "good-quality", "good quality"}};
    default:
        return {};
    }
}

inline bool is_rod_description(int rod, const std::string& text){
    const RodWords words = rod_words(rod);
    bool match = false;
    for (const char* word : words.words){
        match = match || text.find(word) != std::string::npos;
    }
    for (const char* word : words.avoid){
        match = match && text.find(word) == std::string::npos;
    }
    return match;
}


//  What the text box says after a cast (the text as the program reads it,
//  lower case).
enum class Cast{
    NOTHING_YET,    //  The dots, nothing readable yet, or a text still printing.
    NO_BITE,        //  "Not even a nibble..." ("It got away...")
    BITE,           //  "A POKeMON's on the hook!"
    NOT_HERE,       //  PROF. OAK's words: no rod can be cast here.
    OTHER,          //  Some other text.
};

inline int letters_in(const std::string& text){
    int letters = 0;
    for (char ch : text){
        letters += (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ? 1 : 0;
    }
    return letters;
}

//  The bite's text while it still prints (two frames a letter): "a pokemon",
//  "a pokamon's on the h". The beginning of "A POKeMON's on the hook!", its
//  letters alone (the E with its accent is read as "e", as "a", or as no
//  letter at all), one of them read wrong at most. (17 November: read 30
//  times as "a text after the cast that is not of fishing", with a B on it.)
inline bool bite_printing(const std::string& text){
    std::string letters;
    for (char ch : text){
        if (ch >= 'a' && ch <= 'z'){
            letters += ch;
        }
    }
    for (const char* full : {"apokemonsonthehook", "apokmonsonthehook"}){
        const std::string whole = full;
        if (letters.size() < 5 || letters.size() > whole.size()){
            continue;
        }
        int wrong = 0;
        for (size_t i = 0; i < letters.size(); i++){
            wrong += letters[i] != whole[i] ? 1 : 0;
        }
        if (wrong <= 1){
            return true;
        }
    }
    return false;
}

inline Cast read_cast(const std::string& text){
    auto has = [&](const char* word){
        return text.find(word) != std::string::npos;
    };
    if (has("time and place") || has("but not now") || has("for everything")){
        return Cast::NOT_HERE;
    }
    if (has("hook")){
        return Cast::BITE;
    }
    if (has("nibble") || has("not even") || has("got away")){
        return Cast::NO_BITE;
    }
    //  The bag is still closing: the rod's own description.
    if (has("fishing rod") || has("body of water") || has("to fish for")){
        return Cast::NOTHING_YET;
    }
    //  The bite's text is still printing: read again when it is whole.
    if (bite_printing(text)){
        return Cast::NOTHING_YET;
    }
    //  The dots are read as nothing, or as a few stray characters.
    return letters_in(text) < 8 ? Cast::NOTHING_YET : Cast::OTHER;
}


//  A cast brings a bite half the time wherever anything can be fished. That
//  many casts in a row without one (once in a million by chance, and a
//  visit has some hundred casts at most): the player does not fish where
//  the route has it, or the casts are not casts.
const int CASTS_WITHOUT_A_BITE = 20;

inline bool no_bite_at_all(int casts_without_bite){
    return casts_without_bite >= CASTS_WITHOUT_A_BITE;
}

//  The longest a cast takes from the rod's use to its text: the bag closing,
//  a second of waiting, ten dots of 20 frames, the text printing. Past
//  this, something else is on the screen.
const int CAST_SECONDS = 14;


}
}
}
}
#endif

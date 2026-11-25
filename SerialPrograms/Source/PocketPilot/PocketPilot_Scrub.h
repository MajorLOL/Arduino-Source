/*  PocketPilot Scrub
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Text that leaves the program for somewhere else (to Claude, to the
 *  clipboard to paste, to a file to share) is cleaned of what is private:
 *
 *    - the name of the Windows account in a path (C:\Users\<name>\...);
 *    - a Discord webhook address, a Discord bot token;
 *    - the key of the Claude API, a GitHub token;
 *    - the value after "token", "password", "secret", "api key" or
 *      "authorization" (and a "Bearer" token).
 *
 *  It goes by the form of the text, not by where it came from: a line of a
 *  log that happens to hold one is cleaned like any other.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Scrub_H
#define PokemonAutomation_PocketPilot_Scrub_H

#include <QString>

namespace PokemonAutomation{
namespace PocketPilot{


//  The text with what is private taken out, line by line; the rest as it was.
QString scrub_private_text(const QString& text);



}
}
#endif

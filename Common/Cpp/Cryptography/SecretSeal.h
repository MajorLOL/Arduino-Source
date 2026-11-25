/*  Secret Seal
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Keeps a secret (a Discord token, a webhook address) in the settings file
 *  in a form that only the Windows account that wrote it can read: the
 *  Windows data protection API. The sealed text starts with "dpapi1:" and
 *  is the base 64 of what the API gave.
 *
 *  Where there is no such API (not Windows) nothing is sealed: the secret
 *  stays as it was, as text.
 *
 */

#ifndef PokemonAutomation_Cryptography_SecretSeal_H
#define PokemonAutomation_Cryptography_SecretSeal_H

#include <string>

namespace PokemonAutomation{


enum class SecretOpen{
    //  Not a sealed text: it is the secret itself (a file from before this
    //  existed, or from a computer that cannot seal).
    NOT_SEALED,
    //  Opened: "plain" has the secret.
    OPENED,
    //  A sealed text that this account on this computer cannot open (the
    //  file came from another account or computer), or that is damaged.
    FAILED,
};

//  Seals a secret. False when this computer cannot: keep the secret as it is.
bool seal_secret(const std::string& plain, std::string& sealed);

//  Opens a text that was sealed.
SecretOpen open_secret(const std::string& text, std::string& plain);



}
#endif

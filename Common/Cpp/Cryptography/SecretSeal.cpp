/*  Secret Seal
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <cstdint>
#include <cstring>
#include <vector>
#include "SecretSeal.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#endif
#endif

namespace PokemonAutomation{


namespace{

const char PREFIX[] = "dpapi1:";

const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[maybe_unused]] std::string base64_encode(const uint8_t* data, size_t size){
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (size_t c = 0; c < size; c += 3){
        const uint32_t a = data[c];
        const uint32_t b = c + 1 < size ? data[c + 1] : 0;
        const uint32_t d = c + 2 < size ? data[c + 2] : 0;
        const uint32_t word = (a << 16) | (b << 8) | d;
        out += ALPHABET[(word >> 18) & 63];
        out += ALPHABET[(word >> 12) & 63];
        out += c + 1 < size ? ALPHABET[(word >> 6) & 63] : '=';
        out += c + 2 < size ? ALPHABET[word & 63] : '=';
    }
    return out;
}

[[maybe_unused]] bool base64_decode(const std::string& text, std::vector<uint8_t>& out){
    out.clear();
    if (text.size() % 4 != 0){
        return false;
    }
    auto value = [](char ch) -> int{
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9') return ch - '0' + 52;
        if (ch == '+') return 62;
        if (ch == '/') return 63;
        return -1;
    };
    for (size_t c = 0; c < text.size(); c += 4){
        const bool last = c + 4 == text.size();
        int v[4];
        int padding = 0;
        for (int d = 0; d < 4; d++){
            const char ch = text[c + d];
            if (ch == '='){
                //  Only at the end of the last group.
                if (!last || d < 2){
                    return false;
                }
                padding++;
                v[d] = 0;
            }else{
                if (padding != 0){
                    return false;
                }
                v[d] = value(ch);
                if (v[d] < 0){
                    return false;
                }
            }
        }
        const uint32_t word = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) | ((uint32_t)v[2] << 6) | (uint32_t)v[3];
        out.push_back((uint8_t)(word >> 16));
        if (padding < 2){
            out.push_back((uint8_t)(word >> 8));
        }
        if (padding < 1){
            out.push_back((uint8_t)word);
        }
    }
    return true;
}

}


#ifdef _WIN32

namespace{

//  What the sealed text is tied to besides the Windows account: a program
//  that asks the API without it gets nothing out of a copied file.
//  Do not change it with the program's name (Claude GG, PocketPilot Studio
//  before): the keys that a computer has sealed with it cannot be opened with
//  another text.
const char ENTROPY[] = "PocketPilot Studio settings secret";

DATA_BLOB entropy_blob(){
    DATA_BLOB blob;
    blob.pbData = (BYTE*)ENTROPY;
    blob.cbData = (DWORD)(sizeof(ENTROPY) - 1);
    return blob;
}

}

bool seal_secret(const std::string& plain, std::string& sealed){
    DATA_BLOB in;
    in.pbData = (BYTE*)plain.data();
    in.cbData = (DWORD)plain.size();
    DATA_BLOB entropy = entropy_blob();
    DATA_BLOB out;
    out.pbData = nullptr;
    out.cbData = 0;
    if (!CryptProtectData(&in, L"PocketPilot Studio secret", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)){
        return false;
    }
    sealed = std::string(PREFIX) + base64_encode(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}

SecretOpen open_secret(const std::string& text, std::string& plain){
    if (text.compare(0, sizeof(PREFIX) - 1, PREFIX) != 0){
        return SecretOpen::NOT_SEALED;
    }
    std::vector<uint8_t> stored;
    if (!base64_decode(text.substr(sizeof(PREFIX) - 1), stored) || stored.empty()){
        return SecretOpen::FAILED;
    }
    DATA_BLOB in;
    in.pbData = stored.data();
    in.cbData = (DWORD)stored.size();
    DATA_BLOB entropy = entropy_blob();
    DATA_BLOB out;
    out.pbData = nullptr;
    out.cbData = 0;
    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)){
        return SecretOpen::FAILED;
    }
    plain.assign((const char*)out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return SecretOpen::OPENED;
}

#else

bool seal_secret(const std::string&, std::string&){
    return false;
}

SecretOpen open_secret(const std::string& text, std::string&){
    return text.compare(0, sizeof(PREFIX) - 1, PREFIX) == 0 ? SecretOpen::FAILED : SecretOpen::NOT_SEALED;
}

#endif



}

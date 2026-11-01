/*  Pokemon FRLG Autostory Memory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the program's memory grows: how much it grew while each thing was
 *  going on (a battle, a walk, reading text). The memory keeps growing on
 *  some computers during long runs, and only during runs.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMemory_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMemory_H

#include <stdint.h>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Counts the growth while it exists for "activity", apart from the scopes
//  inside it (a battle during a walk counts for the battle, not the walk).
//  "activity" must stay valid (a string literal).
class StoryMemoryScope{
public:
    explicit StoryMemoryScope(const char* activity);
    ~StoryMemoryScope();

    StoryMemoryScope(const StoryMemoryScope&) = delete;
    void operator=(const StoryMemoryScope&) = delete;

private:
    const char* m_activity;
};


//  Start counting (at the start of the program).
void story_memory_reset();

//  "Memory growth since the start: +1200 MB. battle +800 MB (120), ..."
std::string story_memory_summary();



}
}
}
#endif

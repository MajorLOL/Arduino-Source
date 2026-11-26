/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A scripted backend for tests: it records every input and log line.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFakeBackend_H
#define PokemonAutomation_Pokemon_StoryFakeBackend_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a FakeStoryBackend shows.
struct FakeStoryScript{
    //  observe()'s answers in order; the last repeats. None: an empty look.
    std::vector<ScreenObservation> looks = {};
};

//  Records every input and log line; observe() plays the script.
//  code-health: allow public_methods -- the backend calls it fakes plus the three recordings tests read; nothing to split
//  code-health: allow test_only_api -- a test double: the scripted backend the engine and game tests run on
class FakeStoryBackend : public StoryBackend{
public:
    FakeStoryBackend(FakeStoryScript script, CancellableScope& scope);

    //  Each input, in order: "press A", "press +" (by button_to_string()),
    //  "stick 90deg 150ms walk", "stick 90deg 150ms walk, release 300ms"
    //  (a release only when not zero), "wait 100ms".
    const std::vector<std::string>& inputs() const{
        return m_inputs;
    }
    //  Each log line, in order, log_detail()'s too: the file log.
    const std::vector<std::string>& lines() const{
        return m_lines;
    }
    //  The lines log() wrote, in order: the stream's overlay.
    const std::vector<std::string>& overlay_lines() const{
        return m_overlay_lines;
    }

    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;
    virtual ScreenObservation observe() override;
    virtual void log(std::string_view line) override;
    virtual void log_detail(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    FakeStoryScript m_script;
    CancellableScope& m_scope;
    size_t m_next_look = 0;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_lines;
    std::vector<std::string> m_overlay_lines;
};




} // namespace Pokemon
} // namespace PokemonAutomation
#endif

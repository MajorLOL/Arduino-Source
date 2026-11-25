/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryFakeBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


FakeStoryBackend::FakeStoryBackend(FakeStoryScript script, CancellableScope& scope)
    : m_script(std::move(script))
    , m_scope(scope)
{}

void FakeStoryBackend::press(NintendoSwitch::Button button, Milliseconds, Milliseconds){
    m_inputs.push_back(std::format("press {}", NintendoSwitch::button_to_string(button)));
}
void FakeStoryBackend::stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace){
    std::string input = std::format("stick {}deg {}ms {}", degrees, duration.count(), pace == StoryPace::RUN ? "run" : "walk");
    if (release > Milliseconds::zero()){
        input += std::format(", release {}ms", release.count());
    }
    m_inputs.push_back(std::move(input));
}
void FakeStoryBackend::wait(Milliseconds duration){
    m_inputs.push_back(std::format("wait {}ms", duration.count()));
}
ScreenObservation FakeStoryBackend::observe(){
    if (m_script.looks.empty()){
        return {};
    }
    const size_t look = m_next_look < m_script.looks.size() ? m_next_look++ : m_script.looks.size() - 1;
    return m_script.looks[look];
}
void FakeStoryBackend::log(std::string_view line){
    m_lines.emplace_back(line);
    m_overlay_lines.emplace_back(line);
}
void FakeStoryBackend::log_detail(std::string_view line){
    m_lines.emplace_back(line);
}
CancellableScope& FakeStoryBackend::scope(){
    return m_scope;
}



} // namespace Pokemon
} // namespace PokemonAutomation

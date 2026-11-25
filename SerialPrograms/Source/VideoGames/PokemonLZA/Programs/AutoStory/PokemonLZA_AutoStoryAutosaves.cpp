/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <string>
#include <utility>
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryAutosaves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The badge shows 1-3 s.
constexpr Milliseconds AUTOSAVE_DEBOUNCE = 5000ms;
//  Before the steps can have saved: a badge this early is the save before it.
constexpr Milliseconds START_BADGE_WINDOW = 1500ms;

std::optional<std::string_view> step_after(const Pokemon::StoryCheckpoint& checkpoint, std::string_view step){
    for (size_t index = 0; index + 1 < checkpoint.steps.size(); index++){
        if (checkpoint.steps[index].name == step){
            return checkpoint.steps[index + 1].name;
        }
    }
    return std::nullopt;
}

class SightingBackend : public PlzaBackend{
public:
    SightingBackend(PlzaBackend& inner, AutosaveSightings& sightings)
        : m_inner(inner)
        , m_sightings(sightings)
    {}

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        m_inner.press(button, hold, release);
    }
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override{
        m_inner.stick(degrees, duration, release, pace);
    }
    virtual void wait(Milliseconds duration) override{
        m_inner.wait(duration);
    }
    virtual ScreenObservation observe() override{
        ScreenObservation look = m_inner.observe();
        m_sightings.note(m_inner, look);
        return look;
    }
    virtual void log(std::string_view line) override{
        m_inner.log(line);
    }
    virtual void log_detail(std::string_view line) override{
        m_inner.log_detail(line);
    }
    virtual CancellableScope& scope() override{
        return m_inner.scope();
    }

private:
    PlzaBackend& m_inner;
    AutosaveSightings& m_sightings;
};

} // namespace


void AutosaveSightings::restart(){
    m_origin.reset();
    m_seen_in.reset();
    m_last_seen_in.reset();
    //  m_last_badge is kept: a badge still up is the last checkpoint's save.
    m_badge_is_ours = false;
    m_earlier_badge_logged = false;
}

void AutosaveSightings::note(PlzaBackend& backend, const ScreenObservation& look){
    const std::string& step = m_step;
    if (!m_origin){
        m_origin = look.when;
    }
    if (!look.readings.contains(AUTOSAVE_READING)){
        return;
    }
    const bool new_sighting = !m_last_badge || look.when - *m_last_badge >= AUTOSAVE_DEBOUNCE;
    m_last_badge = look.when;
    if (new_sighting){
        m_badge_is_ours = look.when - *m_origin > START_BADGE_WINDOW;
    }
    if (!m_badge_is_ours){
        if (!m_earlier_badge_logged){
            m_earlier_badge_logged = true;
            backend.log(std::format("[PLZA] autosave badge in {}: still up from the last checkpoint's save, not counted", step));
        }
        return;
    }
    if (!m_seen_in){
        m_seen_in = std::string(step);
    }
    m_last_seen_in = std::string(step);
    if (new_sighting){
        const double seconds = (double)std::chrono::duration_cast<Milliseconds>(look.when - *m_origin).count() / 1000;
        backend.log(std::format("[PLZA] autosave badge at {:.1f}s in {}", seconds, step));
    }
}

void AutosaveSightings::saved(PlzaBackend& backend, std::string_view how){
    const std::string& step = m_step;
    if (!m_seen_in){
        m_seen_in = std::string(step);
    }
    m_last_seen_in = std::string(step);
    backend.log(std::format("[PLZA] {} in {}", how, step));
}

std::optional<std::string> AutosaveSightings::finish(std::string_view last_step){
    std::optional<std::string> seen_in = std::move(m_last_seen_in);
    m_last_end_seen = seen_in == last_step;
    restart();
    return seen_in;
}


std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& checkpoint, bool marked_autosave, std::optional<std::string_view> autosave_step
){
    if (!autosave_step){
        if (marked_autosave){
            return std::format("{}: \"save\": \"autosave\" needs \"autosave_step\", the step a run log showed the badge in", checkpoint.id);
        }
        return std::nullopt;
    }
    bool found = false;
    for (const Pokemon::StoryStep& step : checkpoint.steps){
        found = found || step.name == *autosave_step;
    }
    if (!found){
        return std::format("{}: autosave_step {} is not one of its steps", checkpoint.id, *autosave_step);
    }
    if (const std::optional<std::string_view> next = step_after(checkpoint, *autosave_step)){
        return std::format(
            "{}: the game autosaves in {}, before {}: the next checkpoint reloads that save, so end {} at {} and start the next one with {}",
            checkpoint.id, *autosave_step, *next, checkpoint.id, *autosave_step, *next
        );
    }
    return std::nullopt;
}


void note_autosaves(std::span<Pokemon::StoryCheckpoint> checkpoints, AutosaveSightings& sightings){
    for (Pokemon::StoryCheckpoint& checkpoint : checkpoints){
        for (Pokemon::StoryStep& step : checkpoint.steps){
            step.run = [&sightings, name = step.name, run = std::move(step.run)](PlzaBackend& backend, Pokemon::RunState& state) mutable{
                sightings.enter(name);
                SightingBackend noted(backend, sightings);
                return run(noted, state);
            };
        }
    }
}

decltype(Pokemon::StoryGame::save) autosave_policy(MarkedCheckpoints marked, AutosaveSightings& sightings){
    return [marked = std::move(marked), &sightings](PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint) -> std::optional<Pokemon::StoryFailure>{
        const std::string_view last_step = checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.back().name);
        const std::optional<std::string> first_step = sightings.finish(last_step);
        backend.log(std::format(
            "[Boundary] {} end: {}", checkpoint.id, sightings.last_end_seen() ? "badge seen" : "not seen"
        ));
        if (sightings.last_end_seen()){
            backend.log(std::format("[Save] {}: the game autosaved (seen)", checkpoint.id));
            return std::nullopt;
        }
        std::string why = "no autosave seen";
        if (first_step){
            const std::string_view next = step_after(checkpoint, *first_step).value_or(last_step);
            why = std::format("the game autosaved in {}, before {}: a reload would skip {}", *first_step, next, next);
        }else if (marked.autosaves.contains(checkpoint.id)){
            why = "the plan's autosave was not seen";
        }
        backend.log(std::format("[Save] {}: not saved ({})", checkpoint.id, why));
        return std::nullopt;
    };
}

decltype(Pokemon::StoryGame::reload) reloading_only_to_start(Pokemon::StoryGame& game, const AutosaveSightings& sightings){
    return [&game, &sightings](PlzaBackend& backend, const Pokemon::StoryCheckpoint& failed) -> std::optional<std::string>{
        if (const std::optional<std::string>& step = sightings.seen_in()){
            backend.log(std::format(
                "[Save] {}: the game autosaved in {} during the failed run: a reload would load past its start", failed.id, *step
            ));
            return std::nullopt;
        }
        return game.reload(backend, failed);
    };
}

void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint){
    if (const std::optional<std::string>& step = sightings.seen_in()){
        backend.log(std::format(
            "[Save] {}: the game autosaved in {} before the run failed: the latest save is past {}'s start",
            checkpoint, *step, checkpoint
        ));
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

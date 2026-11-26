/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tracks in-game autosave badge sightings and enforces checkpoint save policy.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H
#define PokemonAutomation_PokemonLZA_AutoStoryAutosaves_H

#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Checkpoints marked by their plan's "execution" block, indexed by id.
struct MarkedCheckpoints{
    std::set<std::string, std::less<>> autosaves = {};
    std::set<std::string, std::less<>> reload_when_chained = {};
};

//  Tracks autosave sightings within the active checkpoint.
class AutosaveSightings{
public:
    //  Resets per-checkpoint tracking when a new checkpoint begins.
    void restart();

    //  Records any debounced autosave badge present in `look`.
    void note(PlzaBackend& backend, const ScreenObservation& look);

    //  Records a guaranteed save (e.g. catch autosave) in the current step.
    void saved(PlzaBackend& backend, std::string_view how);

    //  Sets the active step name attributed to subsequent note() and saved() calls.
    void enter(std::string_view step){
        m_step = std::string(step);
    }

    //  Finalizes the checkpoint against `last_step`; returns the step where the last save occurred.
    std::optional<std::string> finish(std::string_view last_step);

    bool last_end_seen() const{
        return m_last_end_seen;
    }

    //  Non-empty once the active checkpoint has advanced the save past its start.
    const std::optional<std::string>& seen_in() const{
        return m_seen_in;
    }

private:
    std::string m_step;
    std::optional<WallClock> m_origin;
    std::optional<WallClock> m_last_badge;
    std::optional<std::string> m_seen_in;
    std::optional<std::string> m_last_seen_in;
    bool m_last_end_seen = false;
    bool m_badge_is_ours = false;
    bool m_earlier_badge_logged = false;
};

//  Validates that `autosave_step` matches `checkpoint`'s last step when required; returns error text or std::nullopt.
std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& checkpoint, bool marked_autosave, std::optional<std::string_view> autosave_step
);

//  Wraps every step in `checkpoints` to report observations to `sightings`, which must outlive them.
void note_autosaves(std::span<Pokemon::StoryCheckpoint> checkpoints, AutosaveSightings& sightings);

//  Returns a StoryGame::save callback that succeeds only when `sightings` recorded a save in the last step.
//  `sightings` must outlive the callback.
decltype(Pokemon::StoryGame::save) autosave_policy(MarkedCheckpoints marked, AutosaveSightings& sightings);

//  Wraps `game.reload` to refuse reloading once `sightings` reports the checkpoint has autosaved.
//  Both `game` and `sightings` must outlive the callback.
decltype(Pokemon::StoryGame::reload) reloading_only_to_start(Pokemon::StoryGame& game, const AutosaveSightings& sightings);

//  Logs where `checkpoint` autosaved after a failure, if its save advanced past the start.
void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

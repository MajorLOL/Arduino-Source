/*  PokemonLZA AutoStory - Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


std::string_view checkpoint_first_action(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.front().action);
}

bool checkpoint_starts_without_save(const Pokemon::StoryCheckpoint& checkpoint){
    const std::string_view action = checkpoint_first_action(checkpoint);
    return action == action_name(PlzaAction::NEW_GAME_INTRO) || action == action_name(PlzaAction::CONFIRM_NEW_GAME);
}

bool checkpoint_makes_new_game_choices(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint_first_action(checkpoint) == action_name(PlzaAction::NEW_GAME_INTRO);
}

namespace{

//  A plan step's action and its scene flags, if any.
struct LeadIn{
    std::string name = {};
    std::string action = {};
    bool stop_at_choice = false;
    bool battle_start = false;
    bool start_window = false;
    bool end_on_black_fade = false;
};

LeadIn lead_in(const JsonValue& event){
    const JsonObject& step = event.to_object_throw();
    LeadIn result{.name = step.get_string_throw("id"), .action = step.get_string_throw("action")};
    if (const JsonObject* args = step.get_object("args")){
        result.stop_at_choice = args->get_boolean_default("stop_at_choice");
        result.battle_start = args->get_boolean_default("battle_start");
        result.start_window = args->get_value("start_window_ms") != nullptr;
        result.end_on_black_fade = args->get_boolean_default("end_on_black_fade");
    }
    return result;
}

//  A scene that needs nothing to start plays inside such a step.
bool plays_to_control(const LeadIn& step){
    return (step.action == action_name(PlzaAction::SCENE) && !step.stop_at_choice && !step.battle_start && !step.end_on_black_fade) ||
        step.action == action_name(PlzaAction::TALK_THROUGH_MAP_SCREEN);
}

//  Throws ParseException naming `where` for an unknown field or a wrong value.
void read_signature_field(const std::string& field, const JsonValue& value, std::string_view where, StartSignature& signature){
    if (field == "party"){
        int64_t slots = 0;
        if (!value.read_integer(slots) || slots < 1 || slots > 6){
            throw ParseException(std::format("{}: start_signature's party is the party bar's filled slots, 1 to 6", where));
        }
        signature.party = (size_t)slots;
        return;
    }
    if (field == "minimap"){
        const std::string* name = value.to_string();
        if (name == nullptr || name->empty()){
            throw ParseException(std::format("{}: start_signature's minimap is a minimap reference's name", where));
        }
        signature.minimap = *name;
        return;
    }
    bool flag = false;
    if (field != "talk_prompt" && field != "wild_zone"){
        throw ParseException(std::format(
            "{}: start_signature has \"{}\"; its fields are talk_prompt, wild_zone, party and minimap", where, field
        ));
    }
    if (!value.read_boolean(flag)){
        throw ParseException(std::format("{}: start_signature's {} is true or false", where, field));
    }
    (field == "talk_prompt" ? signature.talk_prompt : signature.wild_zone) = flag;
}

//  Throws ParseException naming `where` for a malformed or empty one.
std::optional<StartSignature> read_start_signature(const JsonObject& execution, std::string_view where){
    const JsonValue* value = execution.get_value("start_signature");
    if (value == nullptr){
        return std::nullopt;
    }
    const JsonObject* fields = value->to_object();
    if (fields == nullptr || fields->begin() == fields->end()){
        throw ParseException(std::format("{}: start_signature is an object of talk_prompt, wild_zone, party and minimap", where));
    }
    StartSignature signature;
    for (const auto& [field, field_value] : *fields){
        read_signature_field(field, field_value, where, signature);
    }
    return signature;
}

//  Adds `id` to the sets of `marked` the "execution" block's "save" and
//  "reload_when_chained" put it in; `previous`: the checkpoint before it.
void mark_checkpoint(
    const JsonObject& execution, std::string_view id, bool new_game, std::optional<std::string_view> previous, MarkedCheckpoints& marked
){
    const std::string_view where = "execution";
    if (const std::string* save = execution.get_string("save")){
        if (*save == "autosave"){
            marked.autosaves.emplace(id);
        }else if (*save != "unverified"){
            throw ParseException(std::format("{}: save is \"autosave\", \"unverified\" or left out", where));
        }
    }
    if (execution.get_value("reload_when_chained") == nullptr){
        return;
    }
    if (!execution.get_boolean_throw("reload_when_chained", std::string(where))){
        throw ParseException(std::format("{}: reload_when_chained is true or left out", where));
    }
    if (new_game){
        throw ParseException(std::format("{}: reload_when_chained on a checkpoint with no save to reload", where));
    }
    if (!previous || !marked.autosaves.contains(*previous)){
        throw ParseException(std::format(
            "{}: reload_when_chained after a checkpoint that does not end on the game's autosave: the reload would load an older save",
            where
        ));
    }
    marked.reload_when_chained.emplace(id);
}

//  Checks the "execution" block, records start signature and marks in `data`,
//  and checks that an autosave checkpoint names the step it autosaves in.
void read_execution(
    const JsonObject& execution, const StoryCheckpoint& checkpoint, std::optional<std::string_view> previous, StoryData& data
){
    const std::string where = "execution";
    Pokemon::reject_unknown_keys(execution, {"save", "reload_when_chained", "autosave_step", "start_signature"});
    if (std::optional<StartSignature> signature = read_start_signature(execution, where)){
        data.starts.push_back(CheckpointStart{.checkpoint = checkpoint.id, .signature = std::move(*signature)});
    }
    mark_checkpoint(execution, checkpoint.id, checkpoint_starts_without_save(checkpoint), previous, data);
    const JsonValue* step = execution.get_value("autosave_step");
    const std::optional<std::string> autosave_step = step == nullptr ? std::nullopt : std::optional<std::string>(step->to_string_throw(where));
    if (const std::optional<std::string> error = autosave_step_error(checkpoint, data.autosaves.contains(checkpoint.id), autosave_step)){
        throw ParseException(*error);
    }
}

std::vector<std::string> checkpoint_lead_in_errors(const JsonObject& checkpoint){
    std::vector<std::string> errors;
    std::optional<LeadIn> before;
    for (const JsonValue& event : checkpoint.get_array_throw("events")){
        const LeadIn step = lead_in(event);
        if (step.action == action_name(PlzaAction::STORY_BATTLE) &&
            (!before || before->action != action_name(PlzaAction::SCENE) || !before->battle_start)
        ){
            errors.push_back(std::format("{}: a story battle must follow a battle_start scene, which ends on the battle HUD", step.name));
        }
        if (step.action == action_name(PlzaAction::SCENE) && step.start_window && before && plays_to_control(*before)){
            errors.push_back(std::format(
                "{}: a scene must follow what starts it, not {}, which plays its scenes through to control", step.name, before->action
            ));
        }
        before = step;
    }
    return errors;
}

StoryCheckpoint read_plza_checkpoint(
    std::string_view text, std::string_view source, Pokemon::StoryActions& actions, StoryData& data,
    std::optional<std::string_view> previous
){
    const JsonValue file = parse_json(std::string(text));
    const JsonObject* plan = file.to_object() == nullptr ? nullptr : file.to_object()->get_object("checkpoint");
    for (const std::string& error : plan == nullptr ? std::vector<std::string>() : checkpoint_lead_in_errors(*plan)){
        throw ParseException(std::format("{}: {}", source, error));
    }
    StoryCheckpoint checkpoint = Pokemon::read_story_checkpoint(file, "plza", source, actions);
    try{
        read_execution(file.to_object_throw().get_object_throw("execution"), checkpoint, previous, data);
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
    return checkpoint;
}

} // namespace


StoryData load_plza_story(PlzaConsoleHooks& hooks){
    Pokemon::StoryActions actions = plza_story_actions(hooks);
    StoryData data;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        const std::optional<std::string_view> previous = data.checkpoints.empty()
            ? std::nullopt
            : std::optional<std::string_view>(data.checkpoints.back().id);
        data.checkpoints.push_back(read_plza_checkpoint(text, name, actions, data, previous));
    }
    return data;
}

const StoryData& builtin_story_data(){
    static PlzaConsoleHooks no_hooks;
    static const StoryData data = load_plza_story(no_hooks);
    return data;
}

bool builtin_start_reads_map(std::string_view asked){
    const StoryData& data = builtin_story_data();
    for (const Pokemon::StoryCheckpoint& checkpoint : data.checkpoints){
        const CheckpointStart* start = find_start(data.starts, checkpoint.id);
        if (start != nullptr && start->signature.wild_zone){
            return true;
        }
        if (checkpoint.id == asked){
            return false;
        }
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

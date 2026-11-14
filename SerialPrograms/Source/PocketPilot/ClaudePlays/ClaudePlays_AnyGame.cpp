/*  PocketPilot Claude Plays: any game
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "ClaudePlays_AnyGame.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{

using std::chrono::milliseconds;



bool PadLayout::has_button(const std::string& button) const{
    return std::find(buttons.begin(), buttons.end(), button) != buttons.end();
}

const PadLayout& pro_controller_layout(){
    static const PadLayout layout{
        "a Nintendo Switch Pro Controller",
        {
            "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "PLUS", "MINUS", "LSTICK", "RSTICK",
            "UP", "DOWN", "LEFT", "RIGHT",
        },
        {"left_stick", "right_stick"},
        false,
    };
    return layout;
}
const PadLayout& right_joycon_layout(){
    static const PadLayout layout{
        "a single right Joy-Con",
        {
            "A", "B", "X", "Y", "R", "ZR", "PLUS", "STICK",
            "UP", "DOWN", "LEFT", "RIGHT",
        },
        {"stick"},
        true,
    };
    return layout;
}

int picture_tokens(int width, int height){
    if (width <= 0 || height <= 0){
        return 0;
    }
    return ((width + 27) / 28) * ((height + 27) / 28);
}



//  The screen in small.

ScreenThumb screen_thumb(const ImageViewRGB32& picture){
    ScreenThumb thumb;
    const size_t width = picture.width();
    const size_t height = picture.height();
    if (!picture || width < (size_t)ScreenThumb::WIDTH || height < (size_t)ScreenThumb::HEIGHT){
        return thumb;
    }
    thumb.cells.resize((size_t)ScreenThumb::WIDTH * ScreenThumb::HEIGHT * 3);
    size_t at = 0;
    for (int row = 0; row < ScreenThumb::HEIGHT; row++){
        const size_t y0 = height * (size_t)row / ScreenThumb::HEIGHT;
        const size_t y1 = height * (size_t)(row + 1) / ScreenThumb::HEIGHT;
        for (int column = 0; column < ScreenThumb::WIDTH; column++){
            const size_t x0 = width * (size_t)column / ScreenThumb::WIDTH;
            const size_t x1 = width * (size_t)(column + 1) / ScreenThumb::WIDTH;
            //  Every third pixel each way: plenty for an average.
            uint64_t red = 0;
            uint64_t green = 0;
            uint64_t blue = 0;
            uint64_t count = 0;
            for (size_t y = y0; y < y1; y += 3){
                for (size_t x = x0; x < x1; x += 3){
                    const uint32_t pixel = picture.pixel(x, y);
                    red += (pixel >> 16) & 0xff;
                    green += (pixel >> 8) & 0xff;
                    blue += pixel & 0xff;
                    count++;
                }
            }
            if (count == 0){
                count = 1;
            }
            thumb.cells[at++] = (uint8_t)(red / count);
            thumb.cells[at++] = (uint8_t)(green / count);
            thumb.cells[at++] = (uint8_t)(blue / count);
        }
    }
    return thumb;
}

double thumb_difference(const ScreenThumb& first, const ScreenThumb& second){
    if (!first.valid() || !second.valid() || first.cells.size() != second.cells.size()){
        return 1.0;
    }
    //  The average of a cell moves by a few steps of 255 from the noise of
    //  the video alone, and by far more when what it shows changes.
    const int TOLERANCE = 14;
    size_t differ = 0;
    const size_t cells = first.cells.size() / 3;
    for (size_t cell = 0; cell < cells; cell++){
        bool differs = false;
        for (size_t channel = 0; channel < 3; channel++){
            const int a = first.cells[cell * 3 + channel];
            const int b = second.cells[cell * 3 + channel];
            if (std::abs(a - b) > TOLERANCE){
                differs = true;
            }
        }
        differ += differs ? 1 : 0;
    }
    return (double)differ / (double)cells;
}

bool thumb_is_black(const ScreenThumb& thumb){
    if (!thumb.valid()){
        return false;
    }
    const size_t cells = thumb.cells.size() / 3;
    size_t dark = 0;
    for (size_t cell = 0; cell < cells; cell++){
        const int brightest = std::max(
            {thumb.cells[cell * 3], thumb.cells[cell * 3 + 1], thumb.cells[cell * 3 + 2]}
        );
        dark += brightest < 24 ? 1 : 0;
    }
    return dark * 100 >= cells * 98;
}



namespace{


const char* const DIRECTIONS[] = {"UP", "DOWN", "LEFT", "RIGHT"};

bool is_direction(const std::string& button){
    for (const char* direction : DIRECTIONS){
        if (button == direction){
            return true;
        }
    }
    return false;
}

//  A button as Claude may write it, by its name here.
std::string button_name(const std::string& given, const PadLayout& layout){
    std::string name;
    for (char ch : trimmed(given)){
        name += (char)std::toupper((unsigned char)ch);
    }
    for (const char* prefix : {"DPAD_", "DPAD ", "D-PAD ", "BUTTON_"}){
        const std::string start = prefix;
        if (name.size() > start.size() && name.compare(0, start.size(), start) == 0){
            name = name.substr(start.size());
        }
    }
    if (name == "+" || name == "START"){
        return "PLUS";
    }
    if (name == "-" || name == "SELECT"){
        return "MINUS";
    }
    const bool one_stick = layout.has_button("STICK");
    if (name == "LS" || name == "L3" || name == "LCLICK"){
        return one_stick ? "STICK" : "LSTICK";
    }
    if (name == "RS" || name == "R3" || name == "RCLICK"){
        return one_stick ? "STICK" : "RSTICK";
    }
    if (one_stick && (name == "LSTICK" || name == "RSTICK")){
        return "STICK";
    }
    return name;
}

//  The parts of the screen that can be looked at or read on their own.
struct Area{
    const char* name;
    double x;
    double y;
    double width;
    double height;
};
const Area AREAS[] = {
    {"full",            0.00, 0.00, 1.00, 1.00},
    {"top",             0.00, 0.00, 1.00, 0.50},
    {"bottom",          0.00, 0.50, 1.00, 0.50},
    {"left",            0.00, 0.00, 0.50, 1.00},
    {"right",           0.50, 0.00, 0.50, 1.00},
    {"center",          0.25, 0.25, 0.50, 0.50},
    {"top-left",        0.00, 0.00, 0.50, 0.50},
    {"top-right",       0.50, 0.00, 0.50, 0.50},
    {"bottom-left",     0.00, 0.50, 0.50, 0.50},
    {"bottom-right",    0.50, 0.50, 0.50, 0.50},
};

const Area* find_area(const std::string& name){
    for (const Area& area : AREAS){
        if (name == area.name){
            return &area;
        }
    }
    return nullptr;
}

//  ["a","b"]
std::string json_list(const std::vector<std::string>& names){
    std::string list = "[";
    for (const std::string& name : names){
        list += (list.size() > 1 ? ",\"" : "\"") + name + "\"";
    }
    return list + "]";
}

std::string area_list(){
    std::vector<std::string> names;
    for (const Area& area : AREAS){
        names.emplace_back(area.name);
    }
    return json_list(names);
}

std::vector<std::string> button_names(const PadLayout& layout, bool home, bool directions){
    std::vector<std::string> names;
    for (const std::string& button : layout.buttons){
        if (directions || !is_direction(button)){
            names.push_back(button);
        }
    }
    if (home){
        names.emplace_back("HOME");
    }
    return names;
}

std::string spoken_list(const std::vector<std::string>& names){
    std::string list;
    for (const std::string& name : names){
        list += (list.empty() ? "" : ", ") + name;
    }
    return list;
}

//  What every tool that acts takes on top of its own.
std::string with_result_parameters(){
    return
        R"("look":{"type":"string","enum":)" + area_list() +
        R"(,"description":"A picture of this part of the screen comes back with the result (\"full\": all of it)."},)"
        R"("read":{"type":"string","enum":)" + area_list() +
        R"(,"description":"The text the program reads in this part of the screen comes back with the result."})";
}

const char* const STICK_DIRECTIONS =
    R"(["up","up-right","right","down-right","down","down-left","left","up-left"])";

//  The way a stick is pushed, by its name. Also by the names Claude may
//  give it instead ("up_right", "forward", "north-east").
bool stick_vector(const std::string& given, double& x, double& y){
    std::string direction;
    for (char ch : given){
        direction += ch == '_' || ch == ' ' ? '-' : ch;
    }
    struct Other{
        const char* name;
        const char* direction;
    };
    const Other OTHERS[] = {
        {"forward", "up"}, {"forwards", "up"}, {"north", "up"},
        {"back", "down"}, {"backward", "down"}, {"backwards", "down"}, {"south", "down"},
        {"west", "left"}, {"east", "right"},
        {"north-east", "up-right"}, {"north-west", "up-left"},
        {"south-east", "down-right"}, {"south-west", "down-left"},
        {"right-up", "up-right"}, {"left-up", "up-left"},
        {"right-down", "down-right"}, {"left-down", "down-left"},
    };
    for (const Other& other : OTHERS){
        if (direction == other.name){
            direction = other.direction;
        }
    }
    const double diagonal = 0.7071;
    struct Entry{
        const char* name;
        double x;
        double y;
    };
    const Entry ENTRIES[] = {
        {"up",          0, 1},
        {"up-right",    diagonal, diagonal},
        {"right",       1, 0},
        {"down-right",  diagonal, -diagonal},
        {"down",        0, -1},
        {"down-left",   -diagonal, -diagonal},
        {"left",        -1, 0},
        {"up-left",     -diagonal, diagonal},
    };
    for (const Entry& entry : ENTRIES){
        if (direction == entry.name){
            x = entry.x;
            y = entry.y;
            return true;
        }
    }
    return false;
}

std::string lower(const std::string& text){
    std::string out;
    for (char ch : text){
        out += (char)std::tolower((unsigned char)ch);
    }
    return out;
}

//  "under 1%", "34%"
std::string percent(double part){
    const int value = (int)(part * 100.0 + 0.5);
    return value < 1 ? "under 1%" : std::to_string(value) + "%";
}

//  "0.8 s", "12 s"
std::string seconds_text(double seconds){
    if (seconds >= 10){
        return std::to_string((int)(seconds + 0.5)) + " s";
    }
    const int tenths = (int)(seconds * 10.0 + 0.5);
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " s";
}

int64_t integer_between(const QJsonObject& input, const char* name, int64_t fallback, int64_t low, int64_t high){
    const QJsonValue value = input.value(QLatin1String(name));
    if (!value.isDouble()){
        return fallback;
    }
    return std::clamp((int64_t)value.toDouble((double)fallback), low, high);
}

std::string string_of(const QJsonObject& input, const char* name){
    return trimmed(input.value(QLatin1String(name)).toString().toStdString());
}


//  A tool's own part of its result: what it did. An error if it could not
//  be done as asked.
struct Outcome{
    std::string text;
    bool is_error = false;
    //  Something was done on the console (the picture is compared with the
    //  one before).
    bool acted = true;

    static Outcome error(std::string why){
        Outcome outcome;
        outcome.text = std::move(why);
        outcome.is_error = true;
        outcome.acted = false;
        return outcome;
    }
};


//  How the picture changes while nothing is pressed is looked at over this
//  long.
const milliseconds MOTION_WINDOW(200);
//  The part of the cells that differs, below which two pictures are taken
//  for the same one.
const double SAME_PICTURE = 0.01;
const double STILL_PICTURE = 0.005;
//  ... and from which "wait until change" takes the picture for changed.
const double CHANGED_PICTURE = 0.03;


ScreenThumb see(PlayConsole& console){
    ScreenThumb thumb;
    console.screen([&](const ImageViewRGB32& frame){
        thumb = screen_thumb(frame);
    });
    return thumb;
}

//  The picture of an area as it goes to Claude.
std::string area_picture(const ImageViewRGB32& frame, const Area& area, const AnyGameOptions& options){
    const size_t x = (size_t)((double)frame.width() * area.x);
    const size_t y = (size_t)((double)frame.height() * area.y);
    const size_t width = std::max<size_t>(1, std::min(frame.width() - x, (size_t)((double)frame.width() * area.width + 0.5)));
    const size_t height = std::max<size_t>(1, std::min(frame.height() - y, (size_t)((double)frame.height() * area.height + 0.5)));
    const ImageViewRGB32 part = frame.sub_image(x, y, width, height);
    //  The whole screen at the size asked for; a part of it at twice the
    //  detail. Never larger than the video itself.
    const bool whole = area.width >= 1.0 && area.height >= 1.0;
    double out_width = (double)options.picture_width * area.width * (whole ? 1.0 : 2.0);
    out_width = std::min(out_width, (double)width);
    out_width = std::max(out_width, 16.0);
    const double out_height = std::max(16.0, out_width * (double)height / (double)width);
    return Assistant::play_picture(
        part, (int)(out_width + 0.5), (int)(out_height + 0.5), 1,
        Assistant::PlayPictureFormat::JPEG
    );
}

//  What the program reads in an area: its lines, tidied up.
std::string area_text(PlayConsole& console, const ImageViewRGB32& frame, const Area& area){
    const size_t x = (size_t)((double)frame.width() * area.x);
    const size_t y = (size_t)((double)frame.height() * area.y);
    const size_t width = std::max<size_t>(1, std::min(frame.width() - x, (size_t)((double)frame.width() * area.width + 0.5)));
    const size_t height = std::max<size_t>(1, std::min(frame.height() - y, (size_t)((double)frame.height() * area.height + 0.5)));
    const std::string raw = console.read_text(frame.sub_image(x, y, width, height));
    std::string text;
    size_t lines = 0;
    size_t start = 0;
    while (start <= raw.size()){
        size_t end = raw.find('\n', start);
        if (end == std::string::npos){
            end = raw.size();
        }
        const std::string line = one_line(raw.substr(start, end - start), 200);
        if (!line.empty() && lines < 40 && text.size() < 1500){
            text += (text.empty() ? "" : "\n") + line;
            lines++;
        }
        start = end + 1;
    }
    return text;
}

//  What the program sees without help, for the end of a result.
//  "before": the picture before the action. "after" and "later": two
//  pictures after it, MOTION_WINDOW apart.
std::string screen_line(
    const ScreenThumb& before, const ScreenThumb& after, const ScreenThumb& later, bool acted
){
    if (!later.valid()){
        return "Screen: no video comes in from the capture card.";
    }
    if (thumb_is_black(later)){
        return "Screen: black (a fade, or a scene loading).";
    }
    std::string line = "Screen: ";
    if (acted && before.valid()){
        const double changed = thumb_difference(before, later);
        line += changed < SAME_PICTURE
            ? "the same as before the action; "
            : percent(changed) + " of the picture differs from before the action; ";
    }
    const double motion = after.valid() ? thumb_difference(after, later) : 0;
    line += motion < STILL_PICTURE
        ? "still now."
        : "still changing (" + percent(motion) + " of it within 0.2 s).";
    return line;
}


//  The buttons of a call as a list: also when Claude gave one button as a
//  string, or several in one string ("A, B").
QJsonArray button_list(const QJsonValue& given){
    if (!given.isString()){
        return given.toArray();
    }
    QJsonArray list;
    for (const QString& part : given.toString().split(QLatin1Char(','), Qt::SkipEmptyParts)){
        if (!part.trimmed().isEmpty()){
            list.append(part.trimmed());
        }
    }
    return list;
}

//  The buttons of a call, checked.
bool read_buttons(
    const QJsonArray& given, const PadLayout& layout, const AnyGameOptions& options,
    std::vector<std::string>& buttons, std::string& error
){
    for (const QJsonValue& value : given){
        const std::string name = button_name(value.toString().toStdString(), layout);
        if (name == "HOME" && !options.allow_home){
            error =
                "HOME leaves the game for the console's menu, and the user has not allowed it in this "
                "session. Nothing was pressed.";
            return false;
        }
        if (name != "HOME" && !layout.has_button(name)){
            error =
                "There is no button \"" + one_line(name, 20) + "\" on this controller. The buttons are " +
                spoken_list(button_names(layout, options.allow_home, true)) + ". Nothing was pressed.";
            return false;
        }
        buttons.push_back(name);
    }
    return true;
}

//  A direction as the controller takes it: on a controller without a
//  d-pad, a push of the stick.
bool direction_to_stick(const std::string& button, PadInput& input){
    double x = 0;
    double y = 0;
    if (!stick_vector(lower(button), x, y)){
        return false;
    }
    input.stick_x = x;
    input.stick_y = y;
    return true;
}

Outcome press_buttons(PlayConsole& console, const AnyGameOptions& options, const QJsonObject& input){
    const PadLayout& layout = console.layout();
    const QJsonArray given = button_list(input.value("buttons"));
    if (given.isEmpty()){
        return Outcome::error("\"buttons\" has to be a list with at least one button.");
    }
    if (given.size() > 20){
        return Outcome::error("At most 20 buttons in one call.");
    }
    std::vector<std::string> buttons;
    std::string error;
    if (!read_buttons(given, layout, options, buttons, error)){
        return Outcome::error(error);
    }
    const bool hold_given = input.value("hold_ms").isDouble();
    const milliseconds hold_asked(integer_between(input, "hold_ms", 150, 60, 5000));
    const milliseconds gap(integer_between(input, "gap_ms", 300, 100, 5000));
    for (const std::string& button : buttons){
        PadInput pad;
        if (is_direction(button) && layout.directions_are_stick){
            direction_to_stick(button, pad);
        }else{
            pad.buttons.push_back(button);
        }
        //  A direction is pressed shorter than a button: in many games a
        //  longer press moves the cursor twice.
        const milliseconds hold = hold_given ? hold_asked : milliseconds(is_direction(button) ? 110 : 150);
        console.hold(pad, hold);
        console.wait(gap);
    }
    Outcome outcome;
    outcome.text = "Pressed " + spoken_list(buttons) + ".";
    return outcome;
}

Outcome hold_inputs(PlayConsole& console, const AnyGameOptions& options, const QJsonObject& input){
    const PadLayout& layout = console.layout();
    std::vector<std::string> buttons;
    std::string error;
    if (!read_buttons(button_list(input.value("buttons")), layout, options, buttons, error)){
        return Outcome::error(error);
    }
    const double tilt = input.value("tilt").isDouble()
        ? std::clamp(input.value("tilt").toDouble(1.0), 0.2, 1.0)
        : 1.0;
    PadInput pad;
    std::string held;
    //  The sticks.
    for (size_t index = 0; index < layout.sticks.size(); index++){
        const std::string& stick = layout.sticks[index];
        const QJsonValue value = input.value(QString::fromStdString(stick));
        if (value.isUndefined() || value.isNull()){
            continue;
        }
        const std::string direction = lower(trimmed(value.toString().toStdString()));
        double x = 0;
        double y = 0;
        if (!stick_vector(direction, x, y)){
            return Outcome::error(
                "\"" + stick + "\" has to be one of up, up-right, right, down-right, down, down-left, left, "
                "up-left. Nothing was held."
            );
        }
        if (index == 0){
            pad.stick_x = x * tilt;
            pad.stick_y = y * tilt;
        }else{
            pad.stick2_x = x * tilt;
            pad.stick2_y = y * tilt;
        }
        std::string spoken = stick;
        std::replace(spoken.begin(), spoken.end(), '_', ' ');
        held += (held.empty() ? "" : " + ") + spoken + " " + direction;
    }
    //  The buttons. Directions: the d-pad, or the stick where there is none.
    std::vector<std::string> directions;
    for (const std::string& button : buttons){
        if (is_direction(button)){
            directions.push_back(button);
            continue;
        }
        pad.buttons.push_back(button);
        held = held + (held.empty() ? "" : " + ") + button;
    }
    if (!directions.empty()){
        if (layout.directions_are_stick){
            if (pad.stick_x != 0 || pad.stick_y != 0 || directions.size() > 1){
                return Outcome::error(
                    "This controller has no d-pad: a direction is a push of its stick. Give the stick's "
                    "direction instead. Nothing was held."
                );
            }
            direction_to_stick(directions[0], pad);
            pad.stick_x *= tilt;
            pad.stick_y *= tilt;
            held += (held.empty() ? "" : " + ") + std::string("stick ") + lower(directions[0]);
        }else{
            const bool up = std::count(directions.begin(), directions.end(), "UP") > 0;
            const bool down = std::count(directions.begin(), directions.end(), "DOWN") > 0;
            const bool left = std::count(directions.begin(), directions.end(), "LEFT") > 0;
            const bool right = std::count(directions.begin(), directions.end(), "RIGHT") > 0;
            if ((up && down) || (left && right) || directions.size() > 2){
                return Outcome::error(
                    "The d-pad holds one direction, or two next to each other (UP and RIGHT). Nothing was "
                    "held."
                );
            }
            for (const std::string& direction : directions){
                pad.buttons.push_back(direction);
                held += (held.empty() ? "" : " + ") + direction;
            }
        }
    }
    if (held.empty()){
        return Outcome::error("Nothing to hold: give buttons, a stick's direction, or both.");
    }
    const milliseconds duration(integer_between(input, "ms", 500, 50, 10000));
    console.hold(pad, duration);
    console.wait(milliseconds(250));
    Outcome outcome;
    outcome.text = "Held " + held + " for " + std::to_string(duration.count()) + " ms.";
    return outcome;
}

Outcome mash_button(PlayConsole& console, const AnyGameOptions& options, const QJsonObject& input){
    const PadLayout& layout = console.layout();
    std::vector<std::string> buttons;
    std::string error;
    QJsonArray one;
    one.append(input.value("button"));
    if (!read_buttons(one, layout, options, buttons, error)){
        return Outcome::error(error);
    }
    if (is_direction(buttons[0]) || buttons[0] == "HOME"){
        return Outcome::error("mash is for a button like A or B. Nothing was pressed.");
    }
    const int64_t seconds = integer_between(input, "seconds", 3, 1, 60);
    console.mash(buttons[0], milliseconds(seconds * 1000));
    console.wait(milliseconds(300));
    Outcome outcome;
    outcome.text = "Mashed " + buttons[0] + " for " + std::to_string(seconds) + " s.";
    return outcome;
}

Outcome wait_for(PlayConsole& console, const QJsonObject& input){
    const std::string until = lower(string_of(input, "until"));
    if (until.empty()){
        const milliseconds duration(integer_between(input, "ms", 1000, 100, 10000));
        console.wait(duration);
        Outcome outcome;
        outcome.text = "Waited " + std::to_string(duration.count()) + " ms.";
        return outcome;
    }
    if (until != "still" && until != "change"){
        return Outcome::error("\"until\" has to be still or change.");
    }
    const double limit = (double)integer_between(input, "max_seconds", 20, 1, 120);
    const milliseconds every(250);
    const ScreenThumb first = see(console);
    if (!first.valid()){
        return Outcome::error("No video comes in from the capture card: there is nothing to wait for.");
    }
    ScreenThumb previous = first;
    int still_in_row = 0;
    double waited = 0;
    double last_motion = 0;
    Outcome outcome;
    while (waited < limit){
        console.wait(every);
        waited += (double)every.count() / 1000.0;
        const ScreenThumb now = see(console);
        if (!now.valid()){
            return Outcome::error("The video stopped coming in while waiting.");
        }
        if (until == "change"){
            const double changed = thumb_difference(first, now);
            if (changed >= CHANGED_PICTURE){
                outcome.text = "The picture changed after " + seconds_text(waited) + " (" + percent(changed) +
                    " of it).";
                return outcome;
            }
        }else{
            last_motion = thumb_difference(previous, now);
            still_in_row = last_motion < STILL_PICTURE ? still_in_row + 1 : 0;
            if (still_in_row >= 3){
                outcome.text = "The picture is still, " + seconds_text(waited) + " after the wait began.";
                return outcome;
            }
        }
        previous = now;
    }
    outcome.text = until == "change"
        ? "Nothing changed in " + seconds_text(waited) + "."
        : "The picture still changes after " + seconds_text(waited) + " (" + percent(last_motion) +
          " of it between two looks): something on it keeps moving.";
    return outcome;
}


}   //  namespace



std::vector<PlayTool> any_game_tools(
    const PadLayout& layout, const AnyGameOptions& options, bool save_routine
){
    const std::string result_parameters = with_result_parameters();
    const std::string all_buttons = json_list(button_names(layout, options.allow_home, true));
    const std::string area_parameter =
        R"({"type":"object","properties":{"area":{"type":"string","enum":)" + area_list() +
        R"(,"description":"The part of the screen. \"full\" if left out."}}})";

    std::vector<PlayTool> tools;
    tools.push_back(PlayTool{
        "look",
        "A picture of the screen as it is now, or of a part of it at twice the detail. A picture costs far "
        "more tokens than text: use it to see what is where, and read_text to read.",
        area_parameter
    });
    tools.push_back(PlayTool{
        "read_text",
        "The text on the screen, or in a part of it, as the program reads it, line by line from the top: "
        "dialogs, menus, names, numbers. Cheaper and more exact than a picture where there is text. It can "
        "miss stylized text, and it does not say where on the screen a line is.",
        area_parameter
    });
    tools.push_back(PlayTool{
        "press",
        "Presses buttons one after the other: " + spoken_list(button_names(layout, options.allow_home, true)) +
        ". For menus, choices, talking and whatever a single press does. " +
        (layout.directions_are_stick
            ? "UP, DOWN, LEFT and RIGHT are a short push of the stick. "
            : "UP, DOWN, LEFT and RIGHT are the d-pad. ") +
        "Several in one call when you are sure of them.",
        R"({"type":"object","properties":{"buttons":{"type":"array","items":{"type":"string","enum":)" +
        all_buttons + R"(},"description":"1 to 20 buttons, pressed in this order."},)"
        R"("hold_ms":{"type":"integer","description":"How long each is held: 60 to 5000. Leave out for a normal press."},)"
        R"("gap_ms":{"type":"integer","description":"The wait after each press: 100 to 5000. 300 if left out."},)" +
        result_parameters + R"(},"required":["buttons"]})"
    });
    {
        std::string sticks;
        for (const std::string& stick : layout.sticks){
            sticks += "\"" + stick + R"(":{"type":"string","enum":)" + STICK_DIRECTIONS + "},";
        }
        tools.push_back(PlayTool{
            "hold",
            "Holds buttons and sticks together for a time, then lets go: walking and turning the camera in "
            "a 3D game (the sticks), running or charging (a button held), two buttons at once.",
            R"({"type":"object","properties":{"buttons":{"type":"array","items":{"type":"string","enum":)" +
            all_buttons + R"(},"description":"Held together. May be left out."},)" + sticks +
            R"("tilt":{"type":"number","description":"How far the sticks are pushed: 0.2 to 1. 1 if left out."},)"
            R"("ms":{"type":"integer","description":"How long: 50 to 10000."},)" +
            result_parameters + R"(},"required":["ms"]})"
        });
    }
    tools.push_back(PlayTool{
        "mash",
        "Presses one button again and again for some seconds: long texts, and scenes that only want A or B.",
        R"({"type":"object","properties":{"button":{"type":"string","enum":)" +
        json_list(button_names(layout, false, false)) +
        R"(},"seconds":{"type":"integer","description":"1 to 60."},)" +
        result_parameters + R"(},"required":["button","seconds"]})"
    });
    tools.push_back(PlayTool{
        "wait",
        "Waits without pressing anything. \"ms\": for that long. \"until\": \"still\" until the picture stops "
        "changing (a loading screen, an animation, a scene that plays by itself), \"change\" until it "
        "changes (something that is about to happen).",
        R"({"type":"object","properties":{"ms":{"type":"integer","description":"100 to 10000."},)"
        R"("until":{"type":"string","enum":["still","change"]},)"
        R"("max_seconds":{"type":"integer","description":"With \"until\": give up after this long, 1 to 120. 20 if left out."},)" +
        result_parameters + "}}"
    });
    if (options.allow_save && save_routine){
        tools.push_back(PlayTool{
            "save_game",
            "Saves the game with the routine of this game's other programs, starting where the player can "
            "move freely. Only when the goal asks for it, or before something that could go wrong.",
            R"({"type":"object","properties":{)" + result_parameters + "}}"
        });
    }
    return tools;
}

std::string any_game_instructions(
    const PadLayout& layout, const AnyGameOptions& options, const GameFacts& facts, bool save_routine
){
    std::string text =
        "You are Claude, playing a game on a real Nintendo Switch through the program " + PROGRAM_NAME +
        ". The program sees the console's screen through a capture card and works " + layout.name +
        " for you. The user who started it gave you a goal and reads along in the program's log.\n"
        "\n"
        "The game\n";
    if (facts.name.empty()){
        text +=
            "- The program does not know which game is on the screen. Find that out first (look), and say "
            "what you see before you act. The screen can also show a title screen, the console's HOME menu "
            "or no game at all: if the goal cannot be worked on from what is there, say so and call done.\n";
    }else{
        text += "- The game is " + facts.name + ".\n";
    }
    if (!trimmed(facts.notes).empty()){
        text += "- " + trimmed(facts.notes) + "\n";
    }
    if (!trimmed(facts.user_notes).empty()){
        text += "- From the user, about the game: " + trimmed(facts.user_notes) + "\n";
    }
    text +=
        "- The buttons the game shows on its screen are the ones to go by. On a Switch, A is usually yes "
        "and B usually no or back.\n"
        "\n"
        "How to play\n"
        "- You act only through the tools. Each result ends with \"Screen:\": how much of the picture "
        "changed through the action and whether it still moves. That costs almost nothing, and tells "
        "whether a press did anything.\n"
        "- To see what is where, ask for a picture: look, or \"look\" with a tool. To read a dialog, a menu "
        "or a number, read_text (or \"read\" with a tool) costs a fraction of a picture and reads small "
        "print better. A part of the screen (\"bottom\", \"top-left\") comes at twice the detail. The "
        "program's reading can be wrong: when it makes no sense, look.\n"
        "- Ask for less: several buttons in one press when you are sure of them, mash for a long text, "
        "wait with \"until\" for a loading screen, \"look\" or \"read\" with the action instead of a call "
        "of its own.\n"
        "- The program starts the conversation over now and then to save tokens. Only your notes and the "
        "last few actions come along, so keep the notes up to date.\n"
        "\n"
        "Rules\n"
        "- Work toward the goal in steps you can check. Before each tool call, say in one short sentence "
        "what you are about to do and why.\n";
    if (!options.allow_save){
        text +=
            "- Do not save the game: the user has not allowed it in this session. Many games save by "
            "themselves; that cannot be helped.\n";
    }else if (save_routine){
        text +=
            "- Save the game only with save_game, and only when the goal asks for it or before something "
            "that could go wrong.\n";
    }else{
        text +=
            "- Save the game only when the goal asks for it, or before something that could go wrong.\n";
    }
    text +=
        "- Never start a new game over a save, delete save data, or give away, release or sell what the "
        "user has in the game, unless the goal says so. Never buy anything for real money, and stay out of "
        "the console's settings and its shop.\n";
    if (options.allow_home){
        text +=
            "- HOME leaves the game for the console's menu. Press it only when the goal needs that.\n";
    }
    text +=
        "- When the goal is reached, or you see that it cannot be reached from here, call done.\n";
    return text;
}



AnyGame::AnyGame(PlayConsole& console, GameFacts facts, AnyGameOptions options)
    : m_console(console)
    , m_facts(std::move(facts))
    , m_options(options)
{}

std::vector<PlayTool> AnyGame::tools() const{
    return any_game_tools(m_console.layout(), m_options, m_console.can_save());
}
std::string AnyGame::instructions() const{
    return any_game_instructions(m_console.layout(), m_options, m_facts, m_console.can_save());
}

std::string AnyGame::screen(){
    const ScreenThumb first = see(m_console);
    if (!first.valid()){
        return "no video comes in from the capture card.";
    }
    m_console.wait(MOTION_WINDOW);
    const ScreenThumb second = see(m_console);
    if (thumb_is_black(second)){
        return "black (a fade, or a scene loading).";
    }
    return std::string(thumb_difference(first, second) < STILL_PICTURE ? "a still picture" : "a picture that moves") +
        ". The program has not looked at what it shows: look or read_text first.";
}

PlayResult AnyGame::run(const PlayCall& call){
    const QJsonObject input =
        QJsonDocument::fromJson(QByteArray(call.input.data(), (qsizetype)call.input.size())).object();

    //  What is asked for with the result.
    const bool looks = call.name == "look";
    const bool reads = call.name == "read_text";
    std::string look_name = looks ? string_of(input, "area") : string_of(input, "look");
    std::string read_name = reads ? string_of(input, "area") : string_of(input, "read");
    if (looks && look_name.empty()){
        look_name = "full";
    }
    if (reads && read_name.empty()){
        read_name = "full";
    }
    const Area* look_area = look_name.empty() ? nullptr : find_area(lower(look_name));
    const Area* read_area = read_name.empty() ? nullptr : find_area(lower(read_name));

    PlayResult result;
    if ((!look_name.empty() && look_area == nullptr) || (!read_name.empty() && read_area == nullptr)){
        result.is_error = true;
        result.text =
            "There is no part of the screen called \"" +
            one_line(look_area == nullptr && !look_name.empty() ? look_name : read_name, 30) +
            "\". The parts are full, top, bottom, left, right, center, top-left, top-right, bottom-left and "
            "bottom-right. Nothing was done.";
        return result;
    }

    const ScreenThumb before = see(m_console);

    Outcome outcome;
    if (looks || reads){
        outcome.acted = false;
    }else if (call.name == "press"){
        outcome = press_buttons(m_console, m_options, input);
    }else if (call.name == "hold"){
        outcome = hold_inputs(m_console, m_options, input);
    }else if (call.name == "mash"){
        outcome = mash_button(m_console, m_options, input);
    }else if (call.name == "wait"){
        outcome = wait_for(m_console, input);
    }else if (call.name == "save_game" && m_options.allow_save && m_console.can_save()){
        const PlayResult saved = m_console.save_game();
        outcome.text = saved.text;
        outcome.is_error = saved.is_error;
    }else{
        outcome = Outcome::error("There is no tool \"" + one_line(call.name, 40) + "\" in this session.");
    }
    result.is_error = outcome.is_error;
    result.text = outcome.text;

    //  What the screen shows afterwards: two looks a moment apart, and with
    //  the second one the picture and the text that were asked for.
    const ScreenThumb after = see(m_console);
    m_console.wait(MOTION_WINDOW);
    ScreenThumb later;
    std::string words;
    const bool video = m_console.screen([&](const ImageViewRGB32& frame){
        later = screen_thumb(frame);
        if (look_area != nullptr){
            result.image = area_picture(frame, *look_area, m_options);
        }
        if (read_area != nullptr){
            words = area_text(m_console, frame, *read_area);
        }
    });
    result.text += (result.text.empty() ? "" : " ") + screen_line(before, after, later, outcome.acted);
    if (!video){
        return result;
    }
    if (look_area != nullptr){
        result.text += result.image.empty()
            ? " (No picture could be taken.)"
            : std::string(" [A picture of ") + (look_area == &AREAS[0] ? "the screen" : std::string("its ") + look_area->name + " part") + ".]";
    }
    if (read_area != nullptr){
        const std::string where = read_area == &AREAS[0] ? "on the screen" : std::string("in its ") + read_area->name + " part";
        result.text += words.empty()
            ? "\nThe program reads no text " + where + "."
            : "\nText " + where + ", as the program reads it:\n" + words;
    }
    return result;
}



}
}
}

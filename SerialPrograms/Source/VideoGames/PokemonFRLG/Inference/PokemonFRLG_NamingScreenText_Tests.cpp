/*  Pokemon FRLG Naming Screen Text Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The parts of the Box Namer that need no console: the keyboard, the list
 *  of names, which keys to press (on a model of the naming screen written
 *  from the game's source, apart from the code it tests), and the look at a
 *  box's title (on Pokemon Automation's test pictures of the box).
 *  The reader of the name being typed is tested outside the program, on
 *  pictures drawn from the game's font, which is not in this repository.
 *
 */

#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_NamingScreenText.h"
#include "PokemonFRLG_NamingScreenText_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace NamingScreen;

namespace{


class Test_NamingScreenKeyboard : public UnitTest{
public:
    Test_NamingScreenKeyboard() : UnitTest("PokemonFRLG::NamingScreenText - keyboard") {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (columns(PAGE_UPPER) != 8 || columns(PAGE_LOWER) != 8 || columns(PAGE_OTHERS) != 6){
            return "The pages have 8, 8 and 6 columns.";
        }
        if (key(0, 0, 0) != U'A' || key(0, 6, 0) != U' ' || key(0, 7, 0) != U'.' || key(0, 7, 1) != U',' ||
            key(0, 6, 2) != U'S' || key(0, 7, 2) != U' ' || key(0, 6, 3) != U'Z' || key(0, 7, 3) != U' '
        ){
            return "The UPPER page.";
        }
        if (key(1, 0, 0) != U'a' || key(1, 6, 3) != U'z'){
            return "The lower page.";
        }
        if (key(2, 0, 0) != U'0' || key(2, 4, 1) != U'9' || key(2, 5, 0) != U' ' || key(2, 0, 2) != U'!' ||
            key(2, 2, 2) != 0x2642 || key(2, 3, 2) != 0x2640 || key(2, 5, 2) != U'-' ||
            key(2, 0, 3) != 0x2026 || key(2, 1, 3) != 0x201C || key(2, 2, 3) != 0x201D ||
            key(2, 3, 3) != 0x2018 || key(2, 4, 3) != 0x2019 || key(2, 5, 3) != U' '
        ){
            return "The page of the other characters.";
        }
        if (key(2, 6, 0) != 0 || key(3, 0, 0) != 0 || key(0, 0, 4) != 0){
            return "Keys that do not exist.";
        }
        int count = 0;
        for (char32_t c = 1; c < 0x3000; c++){
            count += can_type(c) ? 1 : 0;
        }
        if (count != 76){
            return "The keyboard types 75 characters and the space, not " + std::to_string(count) + ".";
        }
        if (page_for(U'a', 0) != 1 || page_for(U'A', 1) != 0 || page_for(U'5', 0) != 2 ||
            page_for(U'.', 1) != 1 || page_for(U' ', 2) != 2 || page_for(U'_', 0) != -1
        ){
            return "The page to type a character on.";
        }
        int x = -1;
        int y = -1;
        if (!nearest_key(U' ', 0, 0, 3, x, y) || x != 6 || y != 0){
            return "The space from the first key of the bottom row: around the end to the top row.";
        }
        if (!nearest_key(U' ', 0, 7, 2, x, y) || x != 7 || y != 2){
            return "The space under the cursor.";
        }
        if (nearest_key(U'a', 0, 0, 0, x, y)){
            return "No small a on the UPPER page.";
        }
        if (!nearest_visible_key(0, 7, 3, x, y) || key(0, x, y) == U' '){
            return "A character to see next to the empty key.";
        }
        return true;
    }
};


class Test_NamingScreenList : public UnitTest{
public:
    Test_NamingScreenList() : UnitTest("PokemonFRLG::NamingScreenText - list of names") {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<NameLine> lines;
        std::string error;
        //  As the ACE notes publish a code.
        const std::string published =
            "Box  1: C C U n n R E o [CCUnnREo]\n"
            "Box  2: P R o / G w m   [PRo/Gwm]   (change '/' to 'B' for inaccurate emulators)\n"
            "Box  3: A A T S , m     [AATS,m]\r\n"
            "Box  4: E _ F R m _ _ _ [E FRm   ]\n"
            "Box  6: _ F o H I o o r [ FoHIoor]\n"
            "Box  7: x n             [xn]\n"
            "Box 8+: (anything)\n"
            "Box 11: \xE2\x80\xA6 o _ _ _ _ _ _ [\xE2\x80\xA6o      ]\n";
        if (!parse_name_lines(published, 8, lines, error)){
            return "A published code: " + error;
        }
        if (lines.size() != 7 ||
            lines[0].name != U"CCUnnREo" || lines[0].box != 1 || lines[0].line != 1 ||
            lines[1].name != U"PRo/Gwm" || lines[2].name != U"AATS,m" || lines[2].box != 3 ||
            lines[3].name != U"E FRm   " || lines[4].name != U" FoHIoor" || lines[4].box != 6 ||
            lines[5].name != U"xn" || lines[5].line != 6 ||
            lines[6].name.size() != 8 || lines[6].name[0] != 0x2026 || lines[6].name[7] != U' ' || lines[6].box != 11
        ){
            return "A published code is not read as it is meant.";
        }
        if (!parse_name_lines("CCUnnREo\n\n  abc  \n_a b_\nx n\nit's\n", 8, lines, error) || lines.size() != 5){
            return "Plain names: " + error;
        }
        if (lines[1].name != U"abc" || lines[1].line != 3 || lines[2].name != U" a b " || lines[3].name != U"x n" ||
            lines[4].name.size() != 4 || lines[4].name[2] != 0x2019
        ){
            return "Plain names are not read as they are meant.";
        }
        if (!parse_name_lines("[ab ]\n[ a]\n", 8, lines, error) || lines.size() != 2 ||
            lines[0].name != U"ab " || lines[1].name != U" a"
        ){
            return "Names between brackets: " + error;
        }
        struct Refused{
            const char* text;
            const char* reason;
        };
        const Refused REFUSED[] = {
            {"", "No names"},
            {"abcdefghi\n", "9 characters"},
            {"C C U n n R E o\n", "without them"},
            {"Box 1: x n\n", "two ways"},
            {"ab\"c\n", "double quotes"},
            {"ab_\xC3\xA9\n", "keyboard has no"},
            {"___\n", "spaces only"},
            {"[]\n", "no name"},
            {"[abc\n", "without a ]"},
            {"Box 1: a\nb\n", "all of them"},
            {"Box 2: a\nBox 2: b\n", "rising"},
            {"Box 15: a\n", "no box 15"},
            {"Name: a\n", "colon"},
            {"a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\n", "14 boxes"},
            {"\xff\xfe\n", "not text"},
        };
        for (const Refused& refused : REFUSED){
            if (parse_name_lines(refused.text, 8, lines, error)){
                return "Not refused: \"" + std::string(refused.text) + "\".";
            }
            if (error.find(refused.reason) == std::string::npos){
                return "Refused for another reason than \"" + std::string(refused.reason) + "\": " + error;
            }
        }
        return true;
    }
};


//  The naming screen as naming_screen.c runs it: the cursor
//  (HandleDpadMovement, with the column of the three buttons after the
//  last column of keys), the pages (MainState_WaitPageSwap), the name
//  (AddTextCharacter, DeleteTextCharacter, MoveCursorToOKButton).
struct ModelScreen{
    int page = PAGE_UPPER;
    int x = 0;              //  columns(page): on the buttons
    int y = 0;              //  on the buttons: 0 the page, 1 BACK, 2 OK
    int button_id = 0;      //  (the task's tButtonId)
    int places = 8;
    std::u32string name;
    bool done = false;      //  OK was pressed

    void press(Press what){
        if (done){
            return;
        }
        const int last = columns(page);
        switch (what){
        case Press::A:
            if (x < last){
                const char32_t character = key(page, x, y);
                if ((int)name.size() < places){
                    name += character;
                }else{
                    name.back() = character;
                }
                if ((int)name.size() == places){
                    //  The name is full: the cursor to OK.
                    x = last;
                    y = 2;
                }
            }else if (y == 0){
                next_page();
            }else if (y == 1){
                if (!name.empty()){
                    name.pop_back();
                }
            }else{
                done = true;
            }
            break;
        case Press::B:
            if (!name.empty()){
                name.pop_back();
            }
            break;
        case Press::SELECT:
            next_page();
            break;
        case Press::START:
            x = last;
            y = 2;
            break;
        case Press::UP:     move(0, -1); break;
        case Press::DOWN:   move(0, 1); break;
        case Press::LEFT:   move(-1, 0); break;
        case Press::RIGHT:  move(1, 0); break;
        }
    }
    void next_page(){
        const bool on_buttons = x == columns(page);
        page = (page + 1) % PAGES;
        if (on_buttons){
            x = columns(page);
        }else if (x >= columns(page)){
            x = columns(page) - 1;
        }
    }
    void move(int dx, int dy){
        static const int KEY_ROW_TO_BUTTON[4] = {0, 1, 1, 2};
        static const int BUTTON_TO_KEY_ROW[3] = {0, 0, 3};
        const int last = columns(page);
        const int previous_x = x;
        x += dx;
        y += dy;
        if (x < 0){
            x = last;
        }
        if (x > last){
            x = 0;
        }
        if (dx != 0){
            if (x == last){
                button_id = y;
                y = KEY_ROW_TO_BUTTON[y];
            }else if (previous_x == last){
                y = y == 1 ? button_id : BUTTON_TO_KEY_ROW[y];
            }
        }
        if (x == last){
            if (y < 0){
                y = 2;
            }
            if (y >= 3){
                y = 0;
            }
            if (y == 0){
                button_id = 1;
            }else if (y == 2){
                button_id = 2;
            }
        }else{
            if (y < 0){
                y = 3;
            }
            if (y >= 4){
                y = 0;
            }
        }
    }
};

class Test_NamingScreenTyping : public UnitTest{
public:
    Test_NamingScreenTyping() : UnitTest("PokemonFRLG::NamingScreenText - typing") {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::u32string alphabet;
        for (int page = 0; page < PAGES; page++){
            for (int y = 0; y < ROWS; y++){
                for (int x = 0; x < columns(page); x++){
                    const char32_t character = key(page, x, y);
                    if (character != U' ' && alphabet.find(character) == std::u32string::npos){
                        alphabet += character;
                    }
                }
            }
        }
        if (alphabet.size() != 75){
            return "The keyboard has 75 characters to see.";
        }
        unsigned seed = 20261113u;
        auto next = [&]{
            seed = seed * 1103515245u + 12345u;
            return (seed >> 16) & 0x7fff;
        };
        auto random_name = [&]{
            while (true){
                const size_t length = 1 + next() % 8;
                std::u32string name;
                for (size_t i = 0; i < length; i++){
                    name += next() % 5 == 0 ? U' ' : alphabet[next() % alphabet.size()];
                }
                if (name.find_first_not_of(U' ') != std::u32string::npos){
                    return name;
                }
            }
        };

        std::vector<std::u32string> names = {
            U"CCUnnREo", U"PRo/Gwm", U"AATS,m", U"A/FQm", U"DFQm", U" FoHIoor", U"xn", U"E FRm   ", U"E FRm", U".o",
            U"A", U"z", U"9", U" a", U"a ", U"a       ", U"       a", U"abcdefg ", U"ABCDEFG ", U"0123456 ",
            U"a      .", U"Zz9 Zz9 ", U"BOX1", U"BOX14", U"........", U",,,,,,,,", U"a b c d ",
        };
        for (int i = 0; i < 3000; i++){
            names.push_back(random_name());
        }
        for (const std::u32string& name : names){
            //  A screen that has just opened.
            ModelScreen screen;
            std::vector<TypingStep> steps = typing_steps(name, 8, Keys{PAGE_UPPER, 0, 0});
            bool good = !steps.empty();
            for (size_t i = 0; i < steps.size() && good; i++){
                for (Press what : steps[i].presses){
                    screen.press(what);
                }
                good = screen.name == steps[i].shows && screen.page == steps[i].page && !screen.done;
            }
            screen.press(Press::START);
            screen.press(Press::A);
            if (!good || !screen.done || screen.name != name){
                return "Typing " + in_brackets(name) + " on a screen that has just opened gives " + in_brackets(screen.name) + ".";
            }

            //  A screen left anywhere: part of another name, some presses of
            //  anything but A.
            ModelScreen left;
            const std::vector<TypingStep> other = typing_steps(random_name(), 8, Keys{PAGE_UPPER, 0, 0});
            const size_t part = next() % (other.size() + 1);
            for (size_t i = 0; i < part; i++){
                for (Press what : other[i].presses){
                    left.press(what);
                }
            }
            const Press ANY[] = {Press::UP, Press::DOWN, Press::LEFT, Press::RIGHT, Press::SELECT, Press::B, Press::START};
            for (int i = 0; i < 6; i++){
                left.press(ANY[next() % 7]);
            }
            for (Press what : restart_presses(8)){
                left.press(what);
            }
            if (!left.name.empty() || left.x != 0 || left.y != ROWS - 1 || left.done){
                return "The presses to start over do not leave an empty name and the cursor on the first key of the bottom row.";
            }
            steps = typing_steps(name, 8, Keys{left.page, 0, ROWS - 1});
            good = !steps.empty();
            for (size_t i = 0; i < steps.size() && good; i++){
                for (Press what : steps[i].presses){
                    left.press(what);
                }
                good = left.name == steps[i].shows && left.page == steps[i].page && !left.done;
            }
            left.press(Press::START);
            left.press(Press::A);
            if (!good || !left.done || left.name != name){
                return "Typing " + in_brackets(name) + " after starting over gives " + in_brackets(left.name) + ".";
            }
        }
        if (!typing_steps(U"", 8, Keys{}).empty() || !typing_steps(U"abcdefghi", 8, Keys{}).empty() ||
            !typing_steps(U"a_b", 8, Keys{}).empty()
        ){
            return "Steps for a name that cannot be typed.";
        }
        return true;
    }
};


//  A box's title on one of Pokemon Automation's test pictures of the box.
class Test_BoxTitle : public UnitTest{
public:
    Test_BoxTitle(const std::string& image, std::u32string title, std::u32string other)
        : UnitTest("PokemonFRLG::NamingScreenText - box title - " + image)
        , m_image(UNIT_TEST_RESOURCE_PATH() + "PokemonFRLG/Box/" + image)
        , m_title(std::move(title))
        , m_other(std::move(other))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ImageRGB32 image(m_image);
        ImageViewRGB32 game = extract_box_reference(image, GameSettings::instance().GAME_BOX);
        const Pixel pixel = game_pixels(game.data(), game.bytes_per_row(), game.width(), game.height());
        const TitleLook title = look_at_title(pixel, m_title);
        if (!title.shows_name()){
            return "The title " + in_brackets(m_title) + " is not seen: " +
                std::to_string((int)(title.match * 100 + 0.5)) + " in 100 of its pixels.";
        }
        if (look_at_title(pixel, m_other).shows_name()){
            return "The title " + in_brackets(m_title) + " is taken for " + in_brackets(m_other) + " too.";
        }
        return true;
    }

private:
    std::string m_image;
    std::u32string m_title;
    std::u32string m_other;
};


}



void add_tests_NamingScreenText(UnitTestDatabase& database){
    database.add<Test_NamingScreenKeyboard>();
    database.add<Test_NamingScreenList>();
    database.add<Test_NamingScreenTyping>();

    database.add<Test_BoxTitle>("BoxHand_Closed_White.png", U"BOX1", U"BOXI");
    database.add<Test_BoxTitle>("BoxHand_Gold_HD60Pro_NV12.png", U"BOX1", U"B0X1");
    database.add<Test_BoxTitle>("BoxHand_Gold_HD60Pro_YUYV.png", U"BOX1", U"BQX1");
    database.add<Test_BoxTitle>("BoxHand_White_HD60Pro_NV12.png", U"BOX1", U"BOX7");
    database.add<Test_BoxTitle>("BoxHand_White_HD60Pro_YUYV.png", U"BOX1", U"BOX4");
    database.add<Test_BoxTitle>("BoxSelection_Gold.png", U"BOX1", U"BOX");
    database.add<Test_BoxTitle>("BoxSelection_White.png", U"BOX1", U"BOX11");
    database.add<Test_BoxTitle>("CloseSelection_Gold.png", U"BOX1", U"BOX2");
    database.add<Test_BoxTitle>("CloseSelection_White.png", U"BOX1", U"CCUnnREo");
    database.add<Test_BoxTitle>("PartySelection_Gold.png", U"BOX1", U"xn");
    database.add<Test_BoxTitle>("PartySelection_White.png", U"BOX1", U"BOX10");
    database.add<Test_BoxTitle>("BoxSelection_Gold2.png", U"BOX2", U"BOX7");
    database.add<Test_BoxTitle>("BoxSelection_White2.png", U"BOX2", U"BOX3");
    database.add<Test_BoxTitle>("Caterpie_F_4_White.png", U"BOX3", U"BOX8");
    database.add<Test_BoxTitle>("Diglett_F_20_White.png", U"BOX3", U"BOX9");
    database.add<Test_BoxTitle>("nidoran-f-NV12.png", U"BOX3", U"BOX8");
    database.add<Test_BoxTitle>("nidoran-f-YUYV.png", U"BOX3", U"BOX6");
    database.add<Test_BoxTitle>("nidoran-f-mjpeg.png", U"BOX3", U"BOX2");
    database.add<Test_BoxTitle>("nidoran-m-NV12.png", U"BOX3", U"BOX9");
    database.add<Test_BoxTitle>("nidoran-m-YUYV.png", U"BOX3", U"BOX8");
    database.add<Test_BoxTitle>("nidoran-m-mjpeg.png", U"BOX3", U"BOX1");
    database.add<Test_BoxTitle>("Doduo-NV12.png", U"BOX9", U"BOX3");
    database.add<Test_BoxTitle>("Doduo-YUYV.png", U"BOX9", U"BOX8");
    database.add<Test_BoxTitle>("Rhydon-NV12.png", U"BOX6", U"BOX8");
    database.add<Test_BoxTitle>("Rhydon-YUYV.png", U"BOX6", U"BOX3");
    database.add<Test_BoxTitle>("Arcanine_M_100_White.png", U"BOX14", U"BOX13");
    database.add<Test_BoxTitle>("Dragonite_F_100_Whtie.png", U"BOX14", U"BOX11");
    database.add<Test_BoxTitle>("Nidokinq_M_23_White.png", U"BOX14", U"BOX12");
}



}
}
}

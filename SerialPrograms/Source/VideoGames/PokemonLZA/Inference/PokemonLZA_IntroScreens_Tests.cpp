/*  PokemonLZA - Intro Screens Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "NintendoSwitch/Inference/NintendoSwitch_SystemKeyboardDetector.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_Inference_Tests.h"
#include "PokemonLZA_IntroScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

std::string describe(std::optional<size_t> value){
    return value ? std::format("{}", *value) : std::string("none");
}

struct FrameCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<size_t> language_entry = std::nullopt;
        bool look_picker = false;
        std::optional<size_t> look = std::nullopt;
        bool keyboard = false;
        bool home_plza = false;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {.name = "Confirm Choices is none of them", .input = {.file = "AutoStoryConfirmChoices/German_0.png"}, .expectation = {}},
    {.name = "2026-11-15: the language list opens on English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_langlist.png"}, .expectation = {.language_entry = 0}},
    {.name = "2026-11-15: the language list on Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_langsel.png"}, .expectation = {.language_entry = 1}},
    {.name = "2026-11-15: the language list on Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_langsel.png"}, .expectation = {.language_entry = 2}},
    {.name = "2026-11-15: the language list on Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_langsel.png"}, .expectation = {.language_entry = 3}},
    {.name = "2026-11-15: the language list on Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_langsel.png"}, .expectation = {.language_entry = 4}},
    {.name = "2026-11-15: the language list on Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_langsel.png"}, .expectation = {.language_entry = 5}},
    {.name = "2026-11-15: the language list on Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_langsel.png"}, .expectation = {.language_entry = 6}},
    {.name = "2026-11-15: the language list on Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_langsel.png"}, .expectation = {.language_entry = 7}},
    {.name = "2026-11-15: the language list on Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_langsel.png"}, .expectation = {.language_entry = 8}},
    {.name = "2026-11-15: the language list on Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_langsel.png"}, .expectation = {.language_entry = 9}},
    {.name = "2026-11-15: the look picker before the stick moves", .input = {.file = "AutoStoryNewGame/2026-11-15/en_look.png"}, .expectation = {.look_picker = true}},
    {.name = "2026-11-15: the look picker after the first tilt in English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_look_hl.png"}, .expectation = {.look_picker = true, .look = 6}},
    {.name = "2026-11-15: the look picker after the first tilt in Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_look_hl.png"}, .expectation = {.look_picker = true, .look = 7}},
    {.name = "2026-11-15: the keyboard in English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_kb.png"}, .expectation = {.keyboard = true}},
    {.name = "2026-11-15: the keyboard in Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_kb.png"}, .expectation = {.keyboard = true}},
    {.name = "2026-11-15: Simplified Chinese keeps the keyboard after the first +", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_plus0.png"}, .expectation = {.keyboard = true}},
    //  HOME's tiles in last-played order: FireRed first, Z-A second.
    {.name = "2026-11-15: HOME on FireRed's tile", .input = {.file = "AutoStoryNewGame/2026-11-15/home_firered.png"}, .expectation = {}},
    {.name = "2026-11-15: HOME on Z-A's tile", .input = {.file = "AutoStoryNewGame/2026-11-15/home_plza.png"}, .expectation = {.home_plza = true}},
    {.name = "2026-11-18 dark theme: the keyboard in English", .input = {.file = "AutoStoryNewGame/2026-11-18/en_kb_dark.png"}, .expectation = {.keyboard = true}},
    {.name = "2026-11-18 dark theme: HOME on Pokemon HOME's tile", .input = {.file = "AutoStoryNewGame/2026-11-18/home_dark_tile0.png"}, .expectation = {}},
    {.name = "2026-11-18 dark theme: HOME on Z-A's tile", .input = {.file = "AutoStoryNewGame/2026-11-18/home_dark_tile1.png"}, .expectation = {.home_plza = true}},
    {.name = "2026-11-18 dark theme: HOME dimmed", .input = {.file = "AutoStoryNewGame/2026-11-18/home_dark_dim.png"}, .expectation = {}},
};

class Test_IntroScreenFrame : public ImageUnitTest{
public:
    explicit Test_IntroScreenFrame(const FrameCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        const FrameCase::Expectation& expected = m_test.expectation;
        const std::optional<size_t> entry = read_language_list_cursor(frame);
        if (entry != expected.language_entry){
            return std::format("Language list: expected entry {}, read {}.", describe(expected.language_entry), describe(entry));
        }
        if (is_look_picker(frame) != expected.look_picker){
            return std::format("Look picker: expected {}, read {}.", expected.look_picker, !expected.look_picker);
        }
        const std::optional<size_t> look = read_look_picker_cursor(frame);
        if (look != expected.look){
            return std::format("Look picker: expected look {}, read {}.", describe(expected.look), describe(look));
        }
        if (is_name_keyboard(frame) != expected.keyboard){
            return std::format("Name keyboard: expected {}, read {}.", expected.keyboard, !expected.keyboard);
        }
        if (is_home_tile_plza(frame) != expected.home_plza){
            return std::format("HOME tile: expected PLZA {}, read {}.", expected.home_plza, !expected.home_plza);
        }
        return true;
    }

private:
    const FrameCase& m_test;
};


//  The keyboard as type_on_system_keyboard() reads it on opening: the cursor on "1".
struct SystemKeyboardCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
};

const SystemKeyboardCase SYSTEM_KEYBOARD_CASES[] = {
    {.name = "English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_kb.png"}},
    {.name = "Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_kb.png"}},
    {.name = "Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_kb.png"}},
    {.name = "Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_kb.png"}},
    {.name = "Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_kb.png"}},
    {.name = "Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_kb.png"}},
    {.name = "Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_kb.png"}},
    {.name = "Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_kb.png"}},
    {.name = "Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_kb.png"}},
    {.name = "English, dark theme", .input = {.file = "AutoStoryNewGame/2026-11-18/en_kb_dark.png"}},
};

class Test_SystemKeyboardOnNameKeyboard : public ImageUnitTest{
public:
    explicit Test_SystemKeyboardOnNameKeyboard(const SystemKeyboardCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: the system keyboard reader in {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        const std::optional<SystemKeyboardKey> cursor = read_system_keyboard_cursor(frame);
        if (cursor != SystemKeyboardKey{.row = 0, .col = 0}){
            return std::format(
                "Expected the cursor on \"1\" (0, 0), read {}.",
                cursor ? std::format("({}, {})", cursor->row, cursor->col) : std::string("none")
            );
        }
        return true;
    }
};


//  Confirm Choices after the name "1" was submitted: its highlight, the name, and part
//  of the language's name where English OCR reads it.
struct ConfirmCase{
    std::string name;
    struct Input{
        std::string file;   //  Under the test images' PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::string name;
        std::optional<std::string> language;
    };
    Expectation expectation;
};

const ConfirmCase CONFIRM_CASES[] = {
    {.name = "English", .input = {.file = "AutoStoryNewGame/2026-11-15/en_confirm.png"}, .expectation = {.name = "1", .language = "English"}},
    {.name = "Espanol (Espana)", .input = {.file = "AutoStoryNewGame/2026-11-15/esES_confirm.png"}, .expectation = {.name = "1", .language = "Espa"}},
    {.name = "Espanol (Latinoamerica)", .input = {.file = "AutoStoryNewGame/2026-11-15/esLA_confirm.png"}, .expectation = {.name = "1", .language = "Latino"}},
    {.name = "Francais", .input = {.file = "AutoStoryNewGame/2026-11-15/fr_confirm.png"}, .expectation = {.name = "1", .language = "Fran"}},
    {.name = "Deutsch", .input = {.file = "AutoStoryNewGame/2026-11-15/de_confirm.png"}, .expectation = {.name = "1", .language = "Deutsch"}},
    {.name = "Italiano", .input = {.file = "AutoStoryNewGame/2026-11-15/it_confirm.png"}, .expectation = {.name = "1", .language = "Italiano"}},
    {.name = "Japanese", .input = {.file = "AutoStoryNewGame/2026-11-15/ja_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Korean", .input = {.file = "AutoStoryNewGame/2026-11-15/ko_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Simplified Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHans_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "Traditional Chinese", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_confirm.png"}, .expectation = {.name = "1", .language = std::nullopt}},
    {.name = "German, one + after the name", .input = {.file = "AutoStoryNewGame/2026-11-15/de_plus0.png"}, .expectation = {.name = "1", .language = "Deutsch"}},
    {.name = "Traditional Chinese, two + after the name", .input = {.file = "AutoStoryNewGame/2026-11-15/zhHant_plus1.png"}, .expectation = {.name = "1", .language = std::nullopt}},
};

class Test_ConfirmChoicesFrame : public ImageUnitTest{
public:
    explicit Test_ConfirmChoicesFrame(const ConfirmCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - intro screens: Confirm Choices in {}", test.name),
            {std::format("PokemonLZA/{}", test.input.file)}
        )
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& frame = images[0];
        if (!read_confirm_choices_cursor(frame)){
            return std::string("Confirm Choices' highlight did not read.");
        }
        if (is_name_keyboard(frame)){
            return std::string("Read as the name keyboard.");
        }
        const std::string name = read_confirm_choices_name(frame);
        if (name != m_test.expectation.name){
            return std::format("Expected the name \"{}\", read \"{}\".", m_test.expectation.name, name);
        }
        if (m_test.expectation.language){
            const std::string language = read_confirm_choices_language(frame, Language::English);
            if (language.find(*m_test.expectation.language) == std::string::npos){
                return std::format("Expected the language to have \"{}\", read \"{}\".", *m_test.expectation.language, language);
            }
        }
        return true;
    }

private:
    const ConfirmCase& m_test;
};

std::string describe_cursor(std::optional<size_t> cursor){
    return cursor ? std::format("row {}", *cursor) : std::string("no Confirm Choices");
}


//  PokemonLZA/AutoStoryConfirmChoices/<language>_<row>.png reads as its row.
struct LanguageFramesCase{
    std::string name;
    struct Input{
        std::string_view language;
    };
    Input input;
    struct Expectation{
    };
    Expectation expectation;
};

const LanguageFramesCase LANGUAGE_FRAMES_CASES[] = {
    {.name = "English", .input = {.language = "English"}, .expectation = {}},
    {.name = "Spanish", .input = {.language = "Spanish"}, .expectation = {}},
    {.name = "Latin American Spanish", .input = {.language = "SpanishLatAm"}, .expectation = {}},
    {.name = "French", .input = {.language = "French"}, .expectation = {}},
    {.name = "German", .input = {.language = "German"}, .expectation = {}},
    {.name = "Italian", .input = {.language = "Italian"}, .expectation = {}},
    {.name = "Japanese", .input = {.language = "Japanese"}, .expectation = {}},
    {.name = "Korean", .input = {.language = "Korean"}, .expectation = {}},
    {.name = "Simplified Chinese", .input = {.language = "ChineseSimplified"}, .expectation = {}},
    {.name = "Traditional Chinese", .input = {.language = "ChineseTraditional"}, .expectation = {}},
};

std::vector<std::string> language_frame_paths(std::string_view language){
    std::vector<std::string> paths;
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        paths.push_back(std::format("PokemonLZA/AutoStoryConfirmChoices/{}_{}.png", language, row));
    }
    return paths;
}

class Test_ConfirmChoicesFrames : public ImageUnitTest{
public:
    explicit Test_ConfirmChoicesFrames(const LanguageFramesCase& test)
        : ImageUnitTest(
            std::format("PokemonLZA::AutoStory - Confirm Choices - {}", test.name),
            language_frame_paths(test.input.language)
        )
    {}
    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&, const std::vector<ImageRGB32>& images
    ) const override{
        for (size_t row = 0; row < images.size(); row++){
            const std::optional<size_t> cursor = read_confirm_choices_cursor(images[row]);
            if (cursor != row){
                return std::format("{}: expected row {}, read {}.", image_paths()[row], row, describe_cursor(cursor));
            }
        }
        return true;
    }
};

} // namespace


void add_tests_IntroScreens(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<Test_IntroScreenFrame>(test);
    }
    for (const SystemKeyboardCase& test : SYSTEM_KEYBOARD_CASES){
        database.add<Test_SystemKeyboardOnNameKeyboard>(test);
    }
    for (const ConfirmCase& test : CONFIRM_CASES){
        database.add<Test_ConfirmChoicesFrame>(test);
    }
    for (const LanguageFramesCase& test : LANGUAGE_FRAMES_CASES){
        database.add<Test_ConfirmChoicesFrames>(test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

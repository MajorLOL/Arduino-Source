/*  PokemonLZA - Minimap match
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_MinimapMatch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  On 1280x720: centred on (118, 121), 132 px across (the rimmed disk is ~155),
//  in 16 x 16 cells.
const ImageFloatBox MINIMAP_BOX(52.0 / 1280, 55.0 / 720, 132.0 / 1280, 132.0 / 720);
constexpr size_t MINIMAP_CELLS = 16;

//  Cells compared: centres 22 to 62 of the box's 66 px half width, outside the
//  player's arrow and inside the rim's icons. The camera's faint cone stays in.
constexpr double RING_INNER = 22.0 / 66;
constexpr double RING_OUTER = 62.0 / 66;

//  On the 03.04 start (1280x720): 20% darker 2.2, 20% brighter 21.9, moved one cell
//  37-38; 946 frames elsewhere in Main Mission 03 47.5 and up.
constexpr double MINIMAP_MATCH_MAX = 30.0;

//  A screen more than twice as dark or bright as the reference is too far gone.
constexpr double MAX_GAIN = 2.0;

constexpr std::string_view REFERENCE_PREFIX = "plza-minimap-";

//  Throws ParseException naming `source` on a non-hex digit.
FloatPixel hex_pixel(std::string_view hex, size_t offset, std::string_view source){
    const auto channel = [&](size_t index){
        const std::string_view digits = hex.substr(offset + index * 2, 2);
        int value = 0;
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
        if (error != std::errc() || end != digits.data() + digits.size()){
            throw ParseException(std::format("{}: \"{}\" is not a hex color channel.", source, digits));
        }
        return (double)value;
    };
    return FloatPixel(channel(0), channel(1), channel(2));
}

//  `box` of `screen` averaged into `cols` x `rows` cells, row by row.
ReferenceCells grid_cells(const ImageViewRGB32& screen, const ImageFloatBox& box, size_t cols, size_t rows){
    ReferenceCells cells;
    cells.reserve(cols * rows);
    const double cell_width = box.width / (double)cols;
    const double cell_height = box.height / (double)rows;
    for (size_t row = 0; row < rows; row++){
        for (size_t col = 0; col < cols; col++){
            const ImageFloatBox cell(
                box.x + cell_width * (double)col, box.y + cell_height * (double)row,
                cell_width, cell_height
            );
            cells.push_back(image_stats(extract_box_reference(screen, cell)).average);
        }
    }
    return cells;
}

//  RMSD over the cells of the summed squared channel differences (0 = the same, at
//  most 441.7); `cells` is as long as `reference`.
double cells_rmsd(const ReferenceCells& reference, const ReferenceCells& cells){
    double sum = 0;
    for (size_t index = 0; index < cells.size(); index++){
        const double r = reference[index].r - cells[index].r;
        const double g = reference[index].g - cells[index].g;
        const double b = reference[index].b - cells[index].b;
        sum += r * r + g * g + b * b;
    }
    return std::sqrt(sum / (double)cells.size());
}

const std::vector<size_t>& ring_cells(){
    static const std::vector<size_t> ring = []{
        std::vector<size_t> indices;
        for (size_t row = 0; row < MINIMAP_CELLS; row++){
            for (size_t col = 0; col < MINIMAP_CELLS; col++){
                const double x = ((double)col + 0.5) / MINIMAP_CELLS * 2 - 1;
                const double y = ((double)row + 0.5) / MINIMAP_CELLS * 2 - 1;
                const double radius_squared = x * x + y * y;
                if (radius_squared >= RING_INNER * RING_INNER && radius_squared <= RING_OUTER * RING_OUTER){
                    indices.push_back(row * MINIMAP_CELLS + col);
                }
            }
        }
        return indices;
    }();
    return ring;
}

ReferenceCells ring_of(const ReferenceCells& grid){
    ReferenceCells ring;
    ring.reserve(ring_cells().size());
    for (size_t index : ring_cells()){
        ring.push_back(grid[index]);
    }
    return ring;
}

//  Scales each channel's mean to the reference's (within MAX_GAIN), so dusk or shade
//  compares by relative colors. An all-black channel stays black.
ReferenceCells gain_matched(const ReferenceCells& reference, const ReferenceCells& cells){
    FloatPixel reference_sum;
    FloatPixel sum;
    for (size_t index = 0; index < cells.size(); index++){
        reference_sum += reference[index];
        sum += cells[index];
    }
    const auto gain = [](double want, double have){
        if (have <= 0){
            return 1.0;
        }
        return std::clamp(want / have, 1 / MAX_GAIN, MAX_GAIN);
    };
    const double r = gain(reference_sum.r, sum.r);
    const double g = gain(reference_sum.g, sum.g);
    const double b = gain(reference_sum.b, sum.b);
    ReferenceCells matched;
    matched.reserve(cells.size());
    for (const FloatPixel& cell : cells){
        matched.emplace_back(cell.r * r, cell.g * g, cell.b * b);
    }
    return matched;
}

//  Rings by name (the file name after "plza-minimap-"); ParseException if malformed.
const std::map<std::string, ReferenceCells, std::less<>>& minimap_references(){
    static const std::map<std::string, ReferenceCells, std::less<>> references = []{
        std::map<std::string, ReferenceCells, std::less<>> rings;
        for (const auto& [file, text] : autostory_reference_json()){
            if (!file.starts_with(REFERENCE_PREFIX)){
                continue;
            }
            const std::string source = std::format("{}.json", file);
            const JsonValue json = parse_json(text);
            const JsonObject& grid = json.to_object_throw(source).get_object_throw("grid", source);
            rings.emplace(file.substr(REFERENCE_PREFIX.size()), ring_of(read_cell_grid(grid, MINIMAP_CELLS, MINIMAP_CELLS, source)));
        }
        return rings;
    }();
    return references;
}

} // namespace


ReferenceCells read_cell_grid(const JsonObject& grid, size_t cols, size_t rows, std::string_view source){
    const std::string where(source);
    const JsonArray& hex_rows = grid.get_array_throw("rows", where);
    if ((size_t)grid.get_integer_throw("cols", where) != cols || hex_rows.size() != rows){
        throw ParseException(std::format("{}: a grid is not {}x{} cells.", source, cols, rows));
    }
    ReferenceCells cells;
    cells.reserve(cols * rows);
    for (const JsonValue& row : hex_rows){
        const std::string& hex = row.to_string_throw(where);
        if (hex.size() != cols * 6){
            throw ParseException(std::format("{}: a row of a grid is not {} colors.", source, cols));
        }
        for (size_t col = 0; col < cols; col++){
            cells.push_back(hex_pixel(hex, col * 6, source));
        }
    }
    return cells;
}

std::optional<std::string> matching_minimap(const ImageViewRGB32& screen){
    if (minimap_references().empty()){
        return std::nullopt;
    }
    const ReferenceCells ring = ring_of(grid_cells(screen, MINIMAP_BOX, MINIMAP_CELLS, MINIMAP_CELLS));
    std::optional<std::string> best;
    double best_distance = MINIMAP_MATCH_MAX;
    for (const auto& [name, reference] : minimap_references()){
        const double distance = cells_rmsd(reference, gain_matched(reference, ring));
        if (best ? distance < best_distance : distance <= best_distance){
            best = name;
            best_distance = distance;
        }
    }
    return best;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

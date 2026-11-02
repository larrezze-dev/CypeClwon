#pragma once
#include <array>
#include <optional>
#include <string>

namespace cc::core {

/// Rows of the "Categorías" table of the original, in display order (docs/research/ui/categorias_panel.md).
enum class Category { Nodes, Bars, Ties, Columns, Beams, Panels, Shells };
constexpr int kCategoryCount = 7;

/// "Elementos considerados en el análisis" combo (entries taken from the original's web documentation).
enum class AnalysisElements { StressDeformation, Buckling, Modal };

/// State of the "Categorías" panel. Held in memory for the session only (not stored in the .ed3 yet).
struct CategorySettings {
    std::array<bool, kCategoryCount> visible{true, true, true, true, true, true, true};  ///< eye column
    std::array<bool, kCategoryCount> locked{};                                           ///< padlock column
    bool useLayerColours = true;  ///< "Representar la estructura usando los colores de las capas"
    AnalysisElements analysis = AnalysisElements::StressDeformation;

    bool isVisible(Category c) const { return visible[static_cast<int>(c)]; }
    bool isLocked(Category c) const { return locked[static_cast<int>(c)]; }

    // What the viewport honours, as observed in the original (categorias_panel.md): hiding the nodes also hides the
    // support symbols; hiding the bars also hides the nodes and the supports.
    bool nodesDrawn() const { return isVisible(Category::Nodes) && isVisible(Category::Bars); }
    bool supportsDrawn() const { return nodesDrawn(); }
    bool barsDrawn() const { return isVisible(Category::Bars); }
    bool shellsDrawn() const { return isVisible(Category::Shells); }

    bool operator==(const CategorySettings&) const = default;
};

/// JSON text of `settings` (enumeration stored by name).
std::string toJson(const CategorySettings& settings);

/// Parses `jsonText`. Missing fields keep their default; malformed text, wrong types or unknown names give an empty result.
std::optional<CategorySettings> categorySettingsFromJson(const std::string& jsonText);

}  // namespace cc::core

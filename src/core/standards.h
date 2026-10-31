#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Design-code families chosen in "Selección de normas" (docs/research/ui/dialogs_nueva_obra_wizard.md, 3.1).
enum class StandardFamily { Concrete, ColdFormedSteel, HotRolledSteel, Timber, Aluminium };

/// Names available for a family, in the order shown by the original application.
const std::vector<std::string>& availableStandards(StandardFamily family);

/// Name without its trailing " (scope)" suffix, e.g. "Eurocódigo 2 (UE)" -> "Eurocódigo 2".
std::string shortStandardName(std::string name);

/// The code selected for each family. Defaults match the original installation defaults.
struct StandardsSelection {
    std::string concrete = "Eurocódigo 2 (UE)";
    std::string coldFormedSteel = "CTE DB SE-A (España)";
    std::string hotRolledSteel = "Código Estructural (España)";
    std::string timber = "CTE DB SE-M (España)";
    std::string aluminium = "Eurocódigo 9";

    const std::string& get(StandardFamily family) const;
    void set(StandardFamily family, std::string name);

    /// True when every selected name is one of availableStandards() for its family.
    bool isValid() const;

    /// One-line summary shown on the wizard "Normas" button: scope suffixes are removed and the names
    /// are joined as "A, B, C y D" (e.g. "Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9").
    std::string summary() const;
};

}  // namespace cc::core

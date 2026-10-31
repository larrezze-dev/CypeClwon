#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Limit-state combinations chosen in the "Estados límite" dialog
/// (docs/research/ui/dialogs_estados_limite.md). Only the default snow-altitude entry was ever seen on screen, so
/// it is the only choice offered; the editing dialogs behind the pencil icons are a gap (ticket TK-017).
enum class CombinationCode { CTE, EC };

struct LimitStatesSettings {
    static const std::string& defaultSnow();             ///< "Altitud inferior o igual a 1000 m"
    static const std::vector<std::string>& snowChoices();  ///< verified entries only

    std::string groundSnow = defaultSnow();       ///< Tensiones sobre el terreno
    std::string coldFormedSnow = defaultSnow();   ///< E.L.U. de rotura. Acero conformado
    std::string timberSnow = defaultSnow();       ///< E.L.U. de rotura. Madera
    std::string hotRolledSnow = defaultSnow();    ///< E.L.U. de rotura. Acero laminado
    CombinationCode hotRolledCode = CombinationCode::CTE;

    /// One row of "Estados límite a verificar": button caption and the combination shown on its right.
    struct Row {
        std::string caption;
        std::string value;
    };
    /// The eight rows in the order of the original.
    std::vector<Row> rows() const;
    bool isValid() const;
};

}  // namespace cc::core

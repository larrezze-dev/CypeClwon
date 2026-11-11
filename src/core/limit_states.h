#pragma once
#include <string>
#include <vector>

#include "core/standards.h"
#include "core/user_situations.h"

namespace cc::core {

/// Limit-state combinations chosen in the "Estados límite" dialog
/// (docs/research/ui/dialogs_estados_limite.md, docs/research/ui/combinaciones_normas.md sections 12.4 and 13).
/// `Mixed` = third radio of the original, "Hormigón: EC, Acero: CTE".
enum class CombinationCode { CTE, EC, Mixed };

/// The eight buttons of "Estados límite a verificar" in the order of the original.
enum class LimitStateKind { Concrete, Foundation, ColdFormed, HotRolled, Timber, Aluminium, Ground, Displacements };
constexpr int kLimitStateKindCount = 8;
const char* limitStateCaption(LimitStateKind kind);  ///< "Hormigón", "Cimentación", ... (es-ES)

/// Family of a design code for the layout of "Estados límite": Eurocodes, CTE, Código Estructural (and EHE-08 [N]) or any other (no
/// combination data, only the heading line).
enum class NormClass { Eurocode, Cte, StructuralCode, Other };
NormClass normClassOf(const std::string& standardName);

/// One block of "Estados límite": heading lines (kinds in display order), optionally the ground-stress heading, the radios
/// CTE | EC | Hormigón: EC, Acero: CTE (structural-code block) and one snow combo shared by the block.
struct LimitStateBlock {
    NormClass cls = NormClass::Eurocode;
    std::vector<LimitStateKind> kinds;
    bool hasGround = false;  ///< the "Tensiones sobre el terreno" heading and its combo belong to this (Eurocode) block
    bool hasRadios = false;
    bool hasSnow = false;
};

struct LimitStateLayout {
    std::vector<LimitStateBlock> blocks;
    bool groundStandalone = false;  ///< ground stresses shown as "Acciones características" (concrete is not a Eurocode)
    bool ec3Snow = false;           ///< the Eurocode combos offer the three EN 1990 snow zones (every material is a Eurocode)
};

/// Layout deduced from the three variants measured in the original (all Eurocodes; default / CTE button; C.E. button). The grouping
/// rule, section 13 of the research: blocks by norm class in order of the first member (Hormigón, Acero conformado, Acero laminado,
/// Madera, Aluminio); the foundation heading follows the concrete one; ground stresses join the Eurocode block only when the concrete
/// is a Eurocode.
LimitStateLayout buildLimitStateLayout(const StandardsSelection& standards);

struct LimitStatesSettings {
    static const std::string& defaultSnow();               ///< "Altitud inferior o igual a 1000 m"
    static const std::vector<std::string>& snowChoices();  ///< CTE / structural code / aluminium lists (2 entries)
    /// The three EN 1990 zones: "Finlandia, Islandia, Noruega, Suecia", "Resto de los Estados miembro del CEN, H > 1000 m" and "... H <= 1000 m".
    static const std::vector<std::string>& snowChoicesEc();
    /// Returns `value` when it belongs to the list in force, otherwise the equivalent entry of that list (zone preserved; "Nordic" -> > 1000 m
    /// in the two-entry list).
    static std::string normalisedSnow(const std::string& value, bool ec3);

    std::string groundSnow = defaultSnow();       ///< combo of the Eurocode block (ground stresses; also concrete, aluminium...)
    std::string coldFormedSnow = defaultSnow();   ///< combo of the CTE block (cold-formed steel, timber, CTE hot-rolled steel)
    std::string timberSnow = defaultSnow();       ///< mirror of coldFormedSnow (one shared combo)
    std::string hotRolledSnow = defaultSnow();    ///< combo of the structural-code block (concrete and hot-rolled steel)
    CombinationCode hotRolledCode = CombinationCode::CTE;  ///< radios of the structural-code block
    bool codeChosen = false;  ///< false: a structural-code concrete shows the third radio ("Hormigón: EC, Acero: CTE") as in the original

    /// User situations ("Situaciones de proyecto introducidas por el usuario") and which set each limit state uses ("" = Datos generales).
    std::vector<UserSituationSet> userSets;
    std::vector<std::string> userSetOfKind;  ///< E.L.U. set per kind, kLimitStateKindCount entries (shorter = the rest use Datos generales)
    /// E.L.S. (cracking) set per kind: only the kinds with an "E.L.S. de fisuración" row (hasServiceRow) use it. The E.L.U. and E.L.S.
    /// libraries are separate: `userSets` holds both, told apart by UserSituationSet::service [V, research 14.3].
    std::vector<std::string> userServiceSetOfKind;
    /// Concrete and Foundation have the second row "E.L.S. de fisuración" [V, research 14.2/14.3].
    static bool hasServiceRow(LimitStateKind kind);

    std::string userSetFor(LimitStateKind kind) const;
    void setUserSetFor(LimitStateKind kind, const std::string& reference);
    std::string userServiceSetFor(LimitStateKind kind) const;
    void setUserServiceSetFor(LimitStateKind kind, const std::string& reference);
    /// First set of the library (E.L.U. by default, `service` = E.L.S.) with that reference.
    const UserSituationSet* findSet(const std::string& reference, bool service = false) const;
    /// True when some limit state uses the set of that library (the original refuses to delete it, #156484).
    bool setInUse(const std::string& reference, bool service = false) const;
    /// First reference that appears twice in a library, "" if none. The original only detects it when ACCEPTING the limit state: Error
    /// #94676, see repeatedReferenceMessage [V, research 14.4].
    std::string repeatedReference(bool service) const;
    /// Snow entry and code (or "Acciones características") in force for a kind with the given standards.
    struct Effective {
        std::string code;  ///< "EC", "CTE" or "" (none)
        std::string snow;  ///< snow text, empty when the kind has none ("Acciones características")
        bool characteristic = false;
    };
    Effective effective(LimitStateKind kind, const StandardsSelection& standards) const;
    /// Radio in force for a structural-code block (honours `codeChosen`).
    CombinationCode effectiveCode(const StandardsSelection& standards) const;

    /// One row of "Estados límite a verificar": button caption and the combination shown on its right.
    struct Row {
        std::string caption;
        std::string value;
    };
    /// The eight rows with the default standards (kept for compatibility).
    std::vector<Row> rows() const;
    /// The eight rows for the given standards; a limit state with a user set shows its reference.
    std::vector<Row> rows(const StandardsSelection& standards) const;
    bool isValid() const;
};

}  // namespace cc::core

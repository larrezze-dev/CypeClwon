#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// One coefficient of a table; `present == false` is the "-" of the original.
struct Coefficient {
    bool present = false;
    double value = 0.0;
};

struct CoefficientRow {
    std::string label;
    Coefficient favourable, unfavourable, principal, accompanying;
};

struct CoefficientTable {
    std::string title;  ///< "Persistente o transitoria", "Sísmica", ...
    std::vector<CoefficientRow> rows;
};

/// Ultimate limit states show persistent, seismic and accidental formulae; service limit states only one;
/// "Desplazamientos" shows persistent and seismic formulae with two-column tables (partial factors only) whose
/// missing cells are blank instead of "-".
enum class SectionLayout { Ultimate, Service, Displacements };

struct ReportSection {
    std::vector<std::string> titles;  ///< heading lines
    SectionLayout layout = SectionLayout::Ultimate;
    std::vector<CoefficientTable> tables;
};

/// The "Situaciones" report of the original (docs/research/ui/dialogs_opciones_y_pilotes.md, "Captura E").
struct CombinationReport {
    std::string documentName = "Situaciones";
    std::vector<ReportSection> sections;
};

/// Standard names that appear in the section headings.
struct ReportContext {
    std::string concrete = "Eurocódigo 2";
    std::string aluminium = "Eurocódigo 9";
    std::string coldFormed = "CTE DB SE-A";
    std::string timber = "CTE DB SE-M";
};

/// Report behind the first snow pencil of "Estados límite" (concrete, aluminium, ground stresses, cracking).
CombinationReport buildGroundReport(bool highAltitude, const ReportContext& ctx = {});
/// Report behind the second pencil (cold-formed steel and timber).
CombinationReport buildColdFormedTimberReport(bool highAltitude, const ReportContext& ctx = {});

/// Report behind the fourth pencil ("Desplazamientos", characteristic actions); independent of the snow altitude.
/// The pencil of hot-rolled steel (third) opened no viewer in the original (ticket TK-017).
CombinationReport buildDisplacementsReport(bool highAltitude = false, const ReportContext& ctx = {});

/// A legend entry ("Gk  Acción permanente"); `symbol` is the ASCII form used by the text export (g = γ, y = ψ).
struct LegendItem {
    const char* symbol;
    const char* description;
};
const std::vector<LegendItem>& legend(SectionLayout layout);

/// Formulae of the three ultimate situations, in Unicode, for the formats able to show them.
struct Formulae {
    const char* persistent;
    const char* seismic;
    const char* accidental;
    const char* displacementsPersistent;  ///< shown as an image in the viewer, transcribed from the capture
    const char* displacementsSeismic;     ///< not legible in the capture: derived from the persistent one (unverified)
};
const Formulae& formulae();

/// Greek form of an ASCII symbol of the text export: "gQ,1" -> {"γ", "Q,1"}, "Gk" -> {"G", "k"}.
struct SymbolParts {
    std::string base, subscript;
};
SymbolParts prettySymbol(const std::string& ascii);
/// Replaces the ASCII "(g)", "(y)", "(yp)", "(ya)" of the table headers by their Greek form.
std::string prettyText(const std::string& ascii);

/// Intro sentence of every section, as one paragraph.
const char* introText();
std::string formatCoefficient(const Coefficient& c, SectionLayout layout = SectionLayout::Ultimate);  ///< "1.350", "-" (blank in Displacements)

}  // namespace cc::core

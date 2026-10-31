#include "core/combination_report.h"

#include <cstdio>

#include "core/combination_report_data.h"

namespace cc::core {

namespace {

using detail::RawCell;
using detail::RawRow;
using detail::RawTable;

Coefficient toCoefficient(const RawCell& c) { return {c.present, c.value}; }

std::vector<CoefficientTable> tablesOf(const RawTable* tables, int count, bool highAltitude) {
    std::vector<CoefficientTable> out;
    for (int i = 0; i < count; ++i) {
        CoefficientTable t;
        t.title = tables[i].title;
        for (const RawRow& r : tables[i].rows) {
            const RawCell* c = (highAltitude && r.hasHi) ? r.hi : r.lo;
            t.rows.push_back({r.label, toCoefficient(c[0]), toCoefficient(c[1]), toCoefficient(c[2]), toCoefficient(c[3])});
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<CoefficientTable> tableSet(const RawTable* tables, bool highAltitude) {
    // The set sizes are fixed by the generator; look them up through the registry.
    for (int i = 0; i < detail::kTableSetCount; ++i)
        if (detail::kTableSets[i].tables == tables) return tablesOf(tables, detail::kTableSets[i].count, highAltitude);
    return {};
}

}  // namespace

CombinationReport buildGroundReport(bool highAltitude, const ReportContext& ctx) {
    const auto shared = tableSet(detail::kConcreteAluminiumGroundA1, highAltitude);
    CombinationReport r;
    r.sections.push_back({{"E.L.U. de rotura. Hormigón en cimentaciones: " + ctx.concrete,
                           "E.L.U. de rotura. Hormigón: " + ctx.concrete},
                          SectionLayout::Ultimate, shared});
    r.sections.push_back({{"E.L.U. de rotura. Aluminio: " + ctx.aluminium}, SectionLayout::Ultimate, shared});
    r.sections.push_back({{"Tensiones sobre el terreno (A1 - Conjunto B)"}, SectionLayout::Ultimate, shared});
    r.sections.push_back({{"Tensiones sobre el terreno (A2 - Conjunto C)"}, SectionLayout::Ultimate,
                          tableSet(detail::kGroundA2, highAltitude)});
    r.sections.push_back({{"E.L.S. Fisuración. Hormigón en cimentaciones: " + ctx.concrete,
                           "E.L.S. Fisuración. Hormigón: " + ctx.concrete},
                          SectionLayout::Service, tableSet(detail::kServiceQuasiPermanent, highAltitude)});
    return r;
}

CombinationReport buildColdFormedTimberReport(bool highAltitude, const ReportContext& ctx) {
    CombinationReport r;
    r.sections.push_back({{"E.L.U. de rotura. Acero conformado: " + ctx.coldFormed,
                           "E.L.U. de rotura. Madera: " + ctx.timber},
                          SectionLayout::Ultimate, tableSet(detail::kColdFormedTimber, highAltitude)});
    return r;
}

CombinationReport buildDisplacementsReport(bool highAltitude, const ReportContext&) {
    CombinationReport r;
    r.sections.push_back({{"Desplazamientos"}, SectionLayout::Displacements, tableSet(detail::kDisplacements, highAltitude)});
    return r;
}

const std::vector<LegendItem>& legend(SectionLayout layout) {
    static const std::vector<LegendItem> displacements = {
        {"Gk", "Acción permanente"},
        {"Pk", "Acción de pretensado"},
        {"Qk", "Acción variable"},
        {"AE", "Acción sísmica"},
        {"gG", "Coeficiente parcial de seguridad de las acciones permanentes"},
        {"gP", "Coeficiente parcial de seguridad de la acción de pretensado"},
        {"gQ,1", "Coeficiente parcial de seguridad de la acción variable principal"},
        {"gQ,i", "Coeficiente parcial de seguridad de las acciones variables de acompañamiento"},
        {"gAE", "Coeficiente parcial de seguridad de la acción sísmica"}};
    if (layout == SectionLayout::Displacements) return displacements;
    static const std::vector<LegendItem> ultimate = {
        {"Gk", "Acción permanente"},
        {"Pk", "Acción de pretensado"},
        {"Qk", "Acción variable"},
        {"AE", "Acción sísmica"},
        {"Ad", "Acción accidental"},
        {"gG", "Coeficiente parcial de seguridad de las acciones permanentes"},
        {"gP", "Coeficiente parcial de seguridad de la acción de pretensado"},
        {"gQ,1", "Coeficiente parcial de seguridad de la acción variable principal"},
        {"gQ,i", "Coeficiente parcial de seguridad de las acciones variables de acompañamiento"},
        {"gAE", "Coeficiente parcial de seguridad de la acción sísmica"},
        {"gAd", "Coeficiente parcial de seguridad de la acción accidental"},
        {"yp,1", "Coeficiente de combinación de la acción variable principal"},
        {"ya,i", "Coeficiente de combinación de las acciones variables de acompañamiento"}};
    static const std::vector<LegendItem> service = {
        {"Gk", "Acción permanente"},
        {"Pk", "Acción de pretensado"},
        {"Qk", "Acción variable"},
        {"gG", "Coeficiente parcial de seguridad de las acciones permanentes"},
        {"gP", "Coeficiente parcial de seguridad de la acción de pretensado"},
        {"gQ,1", "Coeficiente parcial de seguridad de la acción variable principal"},
        {"gQ,i", "Coeficiente parcial de seguridad de las acciones variables de acompañamiento"},
        {"yp,1", "Coeficiente de combinación de la acción variable principal"},
        {"ya,i", "Coeficiente de combinación de las acciones variables de acompañamiento"}};
    return layout == SectionLayout::Ultimate ? ultimate : service;
}

const Formulae& formulae() {
    // Transcribed from the WMF images of the original DOCX export.
    static const Formulae f = {
        "Σ(j≥1) γG,j·Gk,j + γP·Pk + γQ,1·ψp,1·Qk,1 + Σ(i>1) γQ,i·ψa,i·Qk,i",
        "Σ(j≥1) γG,j·Gk,j + γP·Pk + γAE·AE + Σ(i≥1) γQ,i·ψa,i·Qk,i",
        "Σ(j≥1) γG,j·Gk,j + γP·Pk + γAd·Ad + Σ(i≥1) γQ,i·ψa,i·Qk,i",
        "Σ(j≥1) γG,j·Gk,j + γP·Pk + Σ(i≥1) γQ,i·Qk,i",
        "Σ(j≥1) γG,j·Gk,j + γP·Pk + γAE·AE + Σ(i≥1) γQ,i·Qk,i"};
    return f;
}

SymbolParts prettySymbol(const std::string& ascii) {
    if (ascii.empty()) return {};
    if (ascii[0] == 'g') return {"γ", ascii.substr(1)};
    if (ascii[0] == 'y') return {"ψ", ascii.substr(1)};
    return {ascii.substr(0, 1), ascii.substr(1)};
}

std::string prettyText(const std::string& s) {
    std::string out = s;
    const auto replaceAll = [&out](const std::string& a, const std::string& b) {
        for (std::size_t p = out.find(a); p != std::string::npos; p = out.find(a, p + b.size())) out.replace(p, a.size(), b);
    };
    replaceAll("(yp)", "(ψp)");
    replaceAll("(ya)", "(ψa)");
    replaceAll("(g)", "(γ)");
    replaceAll("(y)", "(ψ)");
    return out;
}

const char* introText() {
    return "Para las distintas situaciones de proyecto, las combinaciones de acciones se definirán de acuerdo con los "
           "siguientes criterios:";
}

std::string formatCoefficient(const Coefficient& c, SectionLayout layout) {
    if (!c.present) return layout == SectionLayout::Displacements ? "" : "-";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3f", c.value);
    return buf;
}

}  // namespace cc::core

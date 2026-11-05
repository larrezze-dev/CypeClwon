#include "core/describir_catalogue.h"

#include <algorithm>
#include <string>

#include "core/predefined_profiles.h"

namespace cc::core {

namespace {

struct Row {
    SectionFamily family;
    ShapeKind kind;
    const char* series;
    const char* profile;
    std::vector<double> mm;   ///< dimensions in mm (order of dimensionKeys())
    std::vector<double> tab;  ///< tabulated A (cm2), Iyy (cm4) and, for the lipped C, Izz (cm4) of the original; empty = none
};

const std::vector<Row>& rows() {
    static const std::vector<Row> all = {
#include "core/describir_catalogue_data.inc"
    };
    return all;
}

struct TieRow {
    ShapeKind kind;
    const char* series;
    const char* profile;
    std::vector<double> mm;
};

const std::vector<TieRow>& tieRows() {
    static const std::vector<TieRow> all = {
#include "core/describir_tie_data.inc"
    };
    return all;
}

Section makeSection(ShapeKind kind, const char* reference, const std::vector<double>& mm, const std::vector<double>& tab = {}) {
    Section s;
    s.reference = reference;
    s.kind = kind;
    for (double v : mm) s.dims.push_back(v / 1000.0);
    if (tab.size() >= 2) {
        s.tabulated.area = tab[0] * 1e-4;  // cm2 -> m2
        s.tabulated.iy = tab[1] * 1e-8;    // cm4 -> m4
    }
    // The lipped C (CF) carries a third catalogue value (Izz) that the original does NOT use: perfil3.dll passes it with the flag off (FUN_1802a6380),
    // so the Izz shown is the computed one. Only A and Iyy are overridden (UF and CF).
    if (tab.size() >= 3 && kind != ShapeKind::ColdFormedC) s.tabulated.iz = tab[2] * 1e-8;
    return s;
}

std::string descriptionOf(const std::string& series, ShapeKind kind) {
    switch (kind) {
    case ShapeKind::BuiltUpDoubleT: return "Perfiles IPC (doble T simétrica armada)";
    case ShapeKind::ColdFormedU: return "Perfiles UF (U conformado simple)";
    case ShapeKind::ColdFormedC: return "Perfiles CF (U conformado rigidizado)";
    case ShapeKind::AluminiumI: return "Perfiles de aluminio (EP) I (doble T)";
    case ShapeKind::TimberRound: return "Madera: redondo macizo";
    case ShapeKind::TimberSquare: return "Madera: cuadrado macizo, serie " + series;
    case ShapeKind::TimberRectangular: return "Madera: rectángulo macizo, serie " + series;
    case ShapeKind::TimberRectangularVariable: return "Madera: rectangular de canto variable";
    default: return series;
    }
}

}  // namespace

const std::vector<ProfileSeries>& describirSeries() {
    static const std::vector<ProfileSeries> all = [] {
        std::vector<ProfileSeries> out;
        for (const Row& row : rows()) {
            auto it = std::find_if(out.begin(), out.end(), [&](const ProfileSeries& s) { return s.reference == row.series; });
            if (it == out.end()) {
                ProfileSeries s;
                s.reference = row.series;
                s.description = descriptionOf(row.series, row.kind);
                s.family = row.family;
                s.kind = row.kind;
                s.source = "Original: capturas manuales (designaciones) y tablas de perfil3.dll (dimensiones, análisis REA)";
                out.push_back(std::move(s));
                it = out.end() - 1;
            }
            it->profiles.push_back(makeSection(row.kind, row.profile, row.mm, row.tab));
        }
        return out;
    }();
    return all;
}

const std::vector<ProfileSeries>& importablePredefinedSeries() {
    static const std::vector<ProfileSeries> all = [] {
        std::vector<ProfileSeries> out = predefinedSeries();
        for (const ProfileSeries& s : describirSeries()) out.push_back(s);
        return out;
    }();
    return all;
}

const std::vector<ShapeKind>& tieShapes() {
    static const std::vector<ShapeKind> shapes = {ShapeKind::AngleEqual, ShapeKind::AngleUnequal, ShapeKind::FlatBar,
                                                  ShapeKind::RoundBar, ShapeKind::SquareBar};
    return shapes;
}

const ProfileSeries* tieSeries(ShapeKind shape) {
    static const std::vector<ProfileSeries> all = [] {
        std::vector<ProfileSeries> out;
        for (ShapeKind kind : tieShapes()) {
            ProfileSeries s;
            s.kind = kind;
            s.family = SectionFamily::HotRolledSteel;
            s.source = "Capturas manuales del original (tirantes)";
            for (const TieRow& row : tieRows()) {
                if (row.kind != kind) continue;
                s.reference = row.series;
                s.description = std::string("Tirantes: ") + row.series;
                s.profiles.push_back(makeSection(kind, row.profile, row.mm));
            }
            out.push_back(std::move(s));
        }
        return out;
    }();
    for (const ProfileSeries& s : all)
        if (s.kind == shape) return &s;
    return nullptr;
}

}  // namespace cc::core

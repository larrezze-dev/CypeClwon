#include "core/predefined_profiles.h"

#include <algorithm>
#include <string>

namespace cc::core {

namespace {

struct Row {
    const char* series;
    const char* reference;
    std::vector<double> mm;  ///< nominal dimensions in millimetres (see the header of the .inc file for the order)
};

const std::vector<Row>& rows() {
    static const std::vector<Row> all = {
#include "core/profile_catalogue_data.inc"
    };
    return all;
}

/// Properties tabulated by the original (pfarcelor.dll / perfil3.dll; docs/research/ui/describir_barras.md section 17) for the
/// predefined profiles it also has. Units of the tables: cm2, cm4, 10^3 cm6 (warping), cm3.
struct TabulatedRow {
    const char* series;
    const char* reference;
    double v[7];  ///< A, Iyy, Izz, It, Iw, Wply, Wplz
};

const std::vector<TabulatedRow>& tabulatedRows() {
    static const std::vector<TabulatedRow> all = {
#include "core/describir_rolled_data.inc"
    };
    return all;
}

struct SeriesDef {
    const char* reference;
    const char* description;
    ShapeKind kind;
};

}  // namespace

const std::vector<ProfileSeries>& predefinedSeries() {
    static const std::vector<ProfileSeries> all = [] {
        const SeriesDef defs[] = {
            {"IPE", "Perfiles IPE (alas paralelas, EN 10365)", ShapeKind::IParallel},
            {"HEA", "Perfiles HEA (alas anchas, serie ligera, EN 10365)", ShapeKind::IParallel},
            {"HEB", "Perfiles HEB (alas anchas, EN 10365)", ShapeKind::IParallel},
            {"HEM", "Perfiles HEM (alas anchas, serie pesada, EN 10365)", ShapeKind::IParallel},
            {"IPN", "Perfiles IPN (alas inclinadas, DIN 1025-1)", ShapeKind::ISloped},
            {"UPN", "Perfiles UPN (alas inclinadas, DIN 1026-1)", ShapeKind::Channel},
            {"L", "Perfiles L de lados iguales (EN 10056-1)", ShapeKind::AngleEqual},
        };
        std::vector<ProfileSeries> out;
        for (const SeriesDef& def : defs) {
            ProfileSeries s;
            s.reference = def.reference;
            s.description = def.description;
            s.family = SectionFamily::HotRolledSteel;
            s.kind = def.kind;
            s.source = "ArcelorMittal, Sections and Merchant Bars (dimensiones nominales); propiedades tabuladas del original (pfarcelor.dll, perfil3.dll) donde existen";
            for (const Row& row : rows()) {
                if (s.reference != row.series) continue;
                Section p;
                p.reference = row.reference;
                p.kind = def.kind;
                for (double v : row.mm) p.dims.push_back(v / 1000.0);
                // The catalogue lists r2 > t for L 75x75x4 ("r2 may be smaller depending on the rolling process"): the toe
                // radius cannot exceed the leg thickness, so it is limited to it.
                if (def.kind == ShapeKind::AngleEqual) p.dims[3] = std::min(p.dims[3], p.dims[1]);
                // The sloped series store the slope as the last dimension, in percent (not a length).
                if (def.kind == ShapeKind::ISloped) p.dims.push_back(kIpnSlopePercent);
                if (def.kind == ShapeKind::Channel) p.dims.push_back(kUpnSlopePercent);
                for (const TabulatedRow& t : tabulatedRows()) {  // tabulated if the original has the profile, otherwise calculated
                    if (s.reference != t.series || p.reference != t.reference) continue;
                    p.tabulated.area = t.v[0] * 1e-4;
                    p.tabulated.iy = t.v[1] * 1e-8;
                    p.tabulated.iz = t.v[2] * 1e-8;
                    p.tabulated.it = t.v[3] * 1e-8;
                    p.tabulated.iw = t.v[4] * 1e-9;
                    p.tabulated.wplY = t.v[5] * 1e-6;
                    p.tabulated.wplZ = t.v[6] * 1e-6;
                    break;
                }
                s.profiles.push_back(std::move(p));
            }
            out.push_back(std::move(s));
        }
        return out;
    }();
    return all;
}

}  // namespace cc::core

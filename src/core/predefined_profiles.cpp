#include "core/predefined_profiles.h"

#include <algorithm>
#include <optional>
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
#include "core/profile_catalogue_rea_data.inc"
    };
    return all;
}

/// Properties tabulated by the original (pfarcelor.dll / perfil3.dll; docs/research/ui/describir_barras.md section 17) for the
/// predefined profiles it also has. Units of the tables: cm2, cm4, 10^3 cm6 (warping), cm3.
struct TabulatedRow {
    const char* series;
    const char* reference;
    double v[8];  ///< A, Iyy, Izz, It, Iw, Wply, Wplz, Avz; -1 = the table of the original has no such value
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
            {"IPE A", "Perfiles IPE A (alas paralelas, serie ligera)", ShapeKind::IParallel},
            {"IPE AA", "Perfiles IPE AA (alas paralelas, serie extraligera)", ShapeKind::IParallel},
            {"IPE O", "Perfiles IPE O (alas paralelas, serie pesada)", ShapeKind::IParallel},
            {"HEAA", "Perfiles HE AA (alas anchas, serie extraligera)", ShapeKind::IParallel},
            {"HE", "Perfiles HE (alas anchas, series de gran tamaño)", ShapeKind::IParallel},
            {"HEC", "Perfiles HE C (alas anchas, serie para pilares)", ShapeKind::IParallel},
            {"HL", "Perfiles HL (alas anchas, series de gran tamaño)", ShapeKind::IParallel},
            {"HD", "Perfiles HD (alas anchas, pilares)", ShapeKind::IParallel},
            {"HP", "Perfiles HP (pilotes)", ShapeKind::IParallel},
            {"UBP", "Perfiles UBP (pilotes, alas paralelas)", ShapeKind::IParallel},
            {"UC", "Perfiles UC (pilares universales, alas paralelas)", ShapeKind::IParallel},
            {"J", "Perfiles J (alas inclinadas)", ShapeKind::ISloped},
            {"UPE", "Perfiles UPE (U de alas paralelas)", ShapeKind::Channel},
            {"PFC", "Perfiles PFC (U de alas paralelas, serie británica)", ShapeKind::Channel},
            {"UB", "Perfiles UB (vigas universales, alas paralelas)", ShapeKind::IParallel},
            {"IPE 750", "Perfiles IPE 750 (alas paralelas)", ShapeKind::IParallel},
            {"IPE V", "Perfiles IPE V (alas paralelas)", ShapeKind::IParallel},
            {"HLZ", "Perfiles HLZ (alas anchas, gran tamaño)", ShapeKind::IParallel},
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
                if (def.kind == ShapeKind::ISloped) {
                    p.dims.push_back(kIpnSlopePercent);
                    // With the assumed slope the toe radius of J 114 x 114 x 27 (7.6 mm) would exceed the thickness at the tip (6.7 mm): limit it.
                    const double tip = p.dims[3] - kIpnSlopePercent / 100.0 * p.dims[1] / 4;
                    p.dims[5] = std::min(p.dims[5], tip);
                }
                // The parallel-flange channels (UPE, PFC) have no slope; J is built by the original with the same helper as the IPN (the slope
                // of the J is NOT read from its own table: the IPN value is assumed, it only affects the non-tabulated derived values).
                if (def.kind == ShapeKind::Channel) p.dims.push_back(s.reference == "UPN" ? kUpnSlopePercent : 0.0);
                for (const TabulatedRow& t : tabulatedRows()) {  // tabulated if the original has the profile, otherwise calculated
                    if (s.reference != t.series || p.reference != t.reference) continue;
                    const auto set = [&t](int i, double unit, std::optional<double>& target) {
                        if (t.v[i] >= 0) target = t.v[i] * unit;
                    };
                    set(0, 1e-4, p.tabulated.area);
                    set(1, 1e-8, p.tabulated.iy);
                    set(2, 1e-8, p.tabulated.iz);
                    set(3, 1e-8, p.tabulated.it);
                    set(4, 1e-9, p.tabulated.iw);
                    set(5, 1e-6, p.tabulated.wplY);
                    set(6, 1e-6, p.tabulated.wplZ);
                    set(7, 1e-4, p.tabulated.avz);  // shear area Avz of the table (I / H / IPN shapes)
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

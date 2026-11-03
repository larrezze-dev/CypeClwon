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
            s.source = "ArcelorMittal, Sections and Merchant Bars (dimensiones nominales)";
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
                s.profiles.push_back(std::move(p));
            }
            out.push_back(std::move(s));
        }
        return out;
    }();
    return all;
}

}  // namespace cc::core

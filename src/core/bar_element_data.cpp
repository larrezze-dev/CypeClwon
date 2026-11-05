#include "core/bar_element_data.h"

#include <cmath>
#include <cstdio>

namespace cc::core {

namespace {

std::string cm(double metres) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f cm", metres * 100.0);
    return buffer;
}

bool inRange(double value, double lo, double hi) { return std::isfinite(value) && value >= lo - 1e-9 && value <= hi + 1e-9; }

std::string rangeMessage(const char* name, double lo, double hi) {
    char buffer[160];
    std::snprintf(buffer, sizeof buffer, "El valor de '%s' debe estar entre %.0f y %.0f.", name, lo * 100.0, hi * 100.0);
    return buffer;
}

}  // namespace

int beamTypeCount(BeamFamily family) {
    switch (family) {
    case BeamFamily::Flat: return 4;
    case BeamFamily::Hung: return 8;
    case BeamFamily::Lattice: return 5;
    case BeamFamily::Prestressed: return 4;
    case BeamFamily::Steel:
    case BeamFamily::Timber: return 0;
    }
    return 0;
}

bool beamBelowSlabOffered(BeamFamily family, int type) {
    switch (family) {
    case BeamFamily::Hung: return type == 0;
    case BeamFamily::Lattice: return type == 4;
    case BeamFamily::Steel:
    case BeamFamily::Timber: return true;
    default: return false;
    }
}

bool beamIsBelowSlab(const BeamData& beam) { return beamBelowSlabOffered(beam.family, beam.type) && beam.belowSlab; }

std::vector<BeamField> beamFields(BeamFamily family, int type) {
    using F = BeamField;
    switch (family) {
    case BeamFamily::Steel:
    case BeamFamily::Timber: return {};
    case BeamFamily::Hung:
        switch (type) {
        case 3:
        case 4:
        case 5:
        case 6: return {F::Width, F::Depth, F::FlangeLeft, F::FlangeRight, F::FlangeDepth};
        case 7: return {F::Width, F::DepthStart, F::DepthEnd};
        default: return {F::Width, F::Depth};
        }
    case BeamFamily::Flat:
    case BeamFamily::Prestressed:
    case BeamFamily::Lattice:
        // types of the three families: 0 rectangular, 1 'T', 2 right flange, 3 left flange (lattice also 4 = hung rectangular)
        switch (type) {
        case 1: return {F::Width, F::Depth, F::FlangeLeft, F::FlangeRight};
        case 2: return {F::Width, F::Depth, F::FlangeRight};
        case 3: return {F::Width, F::Depth, F::FlangeLeft};
        default: return {F::Width, F::Depth};
        }
    }
    return {};
}

BeamSection beamSectionOf(BeamFamily family, int type) {
    // Reinforced concrete beams whose cross section is a plain rectangle (or a trapezoid of variable depth). The T / flanged types,
    // the lattice and prestressed beams and the composite-head variants need shapes the clone does not have yet.
    if (family == BeamFamily::Flat && type == 0) return BeamSection::Rectangular;
    if (family == BeamFamily::Hung && (type == 0 || type == 2)) return BeamSection::Rectangular;
    if (family == BeamFamily::Hung && type == 7) return BeamSection::Variable;
    return BeamSection::Other;
}

std::string validateBeamData(const BeamData& b) {
    if (b.family == BeamFamily::Steel || b.family == BeamFamily::Timber) return {};
    for (BeamField f : beamFields(b.family, b.type)) {
        switch (f) {
        case BeamField::Width:
            if (!inRange(b.widthM, kBeamMinM, kBeamMaxM)) return rangeMessage("Anchura", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::Depth:
            if (!inRange(b.depthM, kBeamMinM, kBeamMaxM)) return rangeMessage("Canto", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::FlangeLeft:
            if (!inRange(b.flangeLeftM, kBeamMinM, kBeamMaxM)) return rangeMessage("Ala izquierda", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::FlangeRight:
            if (!inRange(b.flangeRightM, kBeamMinM, kBeamMaxM)) return rangeMessage("Ala derecha", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::FlangeDepth:
            if (!inRange(b.flangeDepthM, kBeamMinM, kBeamMaxM)) return rangeMessage("Canto del ala", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::DepthStart:
            if (!inRange(b.depthStartM, kBeamMinM, kBeamMaxM)) return rangeMessage("Canto inicial", kBeamMinM, kBeamMaxM);
            break;
        case BeamField::DepthEnd:
            if (!inRange(b.depthEndM, kBeamMinM, kBeamMaxM)) return rangeMessage("Canto final", kBeamMinM, kBeamMaxM);
            break;
        }
    }
    if (b.family == BeamFamily::Hung && (b.type == 3 || b.type == 6)) {
        // #157403, FUN_18005e480: fails when depth - flange depth < 0.0001 (so also when they are equal)
        if (b.depthM - b.flangeDepthM < 0.0001)
            return "El canto del ala (" + cm(b.flangeDepthM) + ") no puede ser mayor que el canto del alma (" + cm(b.depthM) + ").";
    }
    if (b.family == BeamFamily::Hung && b.type == 7) {
        // #404312
        if (std::fabs(b.depthStartM - b.depthEndM) < 0.0001) return "Los valores 'Canto inicial' y 'Canto final' deben ser diferentes.";
    }
    return {};
}

std::string validatePillarData(const PillarData& p, double profileHeightM, double profileWidthM) {
    switch (p.material) {
    case PillarMaterial::Steel: return {};
    case PillarMaterial::Concrete:
        if (p.concreteKind == PillarConcreteKind::Circular) {
            if (!inRange(p.diameterM, kPillarMinM, kPillarMaxM)) return rangeMessage("Diámetro", kPillarMinM, kPillarMaxM);
            return {};
        }
        [[fallthrough]];
    case PillarMaterial::Mixed: {
        const bool circular = p.material == PillarMaterial::Mixed &&
                              (p.mixedKind == MixedPillarKind::CircularEmbeddedProfile || p.mixedKind == MixedPillarKind::CircularTube);
        if (circular) {
            if (!inRange(p.diameterM, kPillarMinM, kPillarMaxM)) return rangeMessage("Diámetro", kPillarMinM, kPillarMaxM);
        } else {
            if (!inRange(p.widthXM, kPillarMinM, kPillarMaxM)) return rangeMessage("Dimensión X", kPillarMinM, kPillarMaxM);
            if (!inRange(p.widthYM, kPillarMinM, kPillarMaxM)) return rangeMessage("Dimensión Y", kPillarMinM, kPillarMaxM);
        }
        if (p.material == PillarMaterial::Mixed && profileHeightM > 0.0 && profileWidthM > 0.0) {
            if (p.mixedKind == MixedPillarKind::RectangularEmbeddedProfile &&
                (profileHeightM > p.widthYM + 1e-9 || profileWidthM > p.widthXM + 1e-9))
                return "El perfil metálico no está contenido dentro de la sección de hormigón.";
            if (p.mixedKind == MixedPillarKind::CircularEmbeddedProfile &&
                std::hypot(profileHeightM, profileWidthM) > p.diameterM + 1e-9)
                return "El perfil metálico no está contenido dentro de la sección de hormigón.";
        }
        return {};
    }
    }
    return {};
}

}  // namespace cc::core

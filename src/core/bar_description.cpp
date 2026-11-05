#include "core/bar_description.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cc::core {

namespace {

struct IconRow {
    BarMaterialIcon icon;
    const char* key;
    const char* nameEs;
};

constexpr IconRow kIcons[] = {
    {BarMaterialIcon::RolledSteel, "rolledSteel", "Acero laminado"},
    {BarMaterialIcon::RolledSteelFamily2, "rolledSteelFamily2", "Acero laminado y armado (serie IPC)"},
    {BarMaterialIcon::ColdFormedSteel, "coldFormedSteel", "Acero conformado"},
    {BarMaterialIcon::Timber, "timber", "Madera"},
    {BarMaterialIcon::ExtrudedAluminium, "extrudedAluminium", "Aluminio extruido"},
    {BarMaterialIcon::SpecialAluminium, "specialAluminium", "Secciones especiales de aluminio"},
    {BarMaterialIcon::Concrete, "concrete", "Hormigón"},
    {BarMaterialIcon::Generic, "generic", "Sección genérica"},
};

struct ArrangementRow {
    Arrangement arrangement;
    const char* key;
    const char* nameEs;
};

constexpr ArrangementRow kArrangements[] = {
    {Arrangement::Simple, "simple", "Perfil simple"},
    {Arrangement::SimpleWithHaunches, "simpleWithHaunches", "Simple con cartelas"},
    {Arrangement::WithConcreteSlab, "withConcreteSlab", "Con losa de hormigón"},
    {Arrangement::DoubleBoxWelded, "doubleBoxWelded", "Doble en cajón soldado"},
    {Arrangement::DoubleBoxBattened, "doubleBoxBattened", "Doble en cajón con presillas"},
    {Arrangement::DoubleBoxGeneric, "doubleBoxGeneric", "Doble en cajón unión genérica"},
    {Arrangement::HalfProfile, "halfProfile", "Medio perfil"},
    {Arrangement::SideFlatPlates, "sideFlatPlates", "Con platabandas laterales"},
    {Arrangement::Boyd, "boyd", "Boyd"},
    {Arrangement::DoubleIWelded, "doubleIWelded", "Doble en I unión soldada"},
    {Arrangement::DoubleIBattened, "doubleIBattened", "Doble en I con presillas"},
    {Arrangement::DoubleIGeneric, "doubleIGeneric", "Doble en I unión genérica"},
};

struct BehaviourRow {
    BehaviourKind behaviour;
    const char* key;
    const char* nameEs;
};

constexpr BehaviourRow kBehaviours[] = {
    {BehaviourKind::Linear, "linear", "Lineal"},
    {BehaviourKind::TensionOnly, "tensionOnly", "Sólo tracción"},
    {BehaviourKind::CompressionOnly, "compressionOnly", "Sólo compresión"},
};

bool isIShape(ShapeKind shape) { return shape == ShapeKind::IParallel || shape == ShapeKind::ISloped; }

/// The I icon of the original covers both I families (parallel and inclined flanges): the series decides.
ShapeKind shapeGroup(ShapeKind s) { return s == ShapeKind::ISloped ? ShapeKind::IParallel : s; }

bool finitePositive(double v) { return std::isfinite(v) && v > 0.0; }

}  // namespace

const char* materialIconKey(BarMaterialIcon icon) {
    for (const IconRow& r : kIcons)
        if (r.icon == icon) return r.key;
    return "";
}
std::optional<BarMaterialIcon> materialIconFromKey(const std::string& key) {
    for (const IconRow& r : kIcons)
        if (key == r.key) return r.icon;
    return std::nullopt;
}
const char* materialIconNameEs(BarMaterialIcon icon) {
    for (const IconRow& r : kIcons)
        if (r.icon == icon) return r.nameEs;
    return "";
}
const char* arrangementKey(Arrangement a) {
    for (const ArrangementRow& r : kArrangements)
        if (r.arrangement == a) return r.key;
    return "";
}
std::optional<Arrangement> arrangementFromKey(const std::string& key) {
    for (const ArrangementRow& r : kArrangements)
        if (key == r.key) return r.arrangement;
    return std::nullopt;
}
const char* arrangementNameEs(Arrangement a) {
    for (const ArrangementRow& r : kArrangements)
        if (r.arrangement == a) return r.nameEs;
    return "";
}
const char* behaviourKey(BehaviourKind b) {
    for (const BehaviourRow& r : kBehaviours)
        if (r.behaviour == b) return r.key;
    return "";
}
std::optional<BehaviourKind> behaviourFromKey(const std::string& key) {
    for (const BehaviourRow& r : kBehaviours)
        if (key == r.key) return r.behaviour;
    return std::nullopt;
}
const char* behaviourNameEs(BehaviourKind b) {
    for (const BehaviourRow& r : kBehaviours)
        if (r.behaviour == b) return r.nameEs;
    return "";
}
const std::vector<BarMaterialIcon>& allMaterialIcons() {
    static const std::vector<BarMaterialIcon> all = [] {
        std::vector<BarMaterialIcon> out;
        for (const IconRow& r : kIcons) out.push_back(r.icon);
        return out;
    }();
    return all;
}

std::optional<SectionFamily> familyOfIcon(BarMaterialIcon icon) {
    switch (icon) {
    case BarMaterialIcon::RolledSteel: return SectionFamily::HotRolledSteel;
    case BarMaterialIcon::RolledSteelFamily2: return SectionFamily::BuiltUpSteel;
    case BarMaterialIcon::ColdFormedSteel: return SectionFamily::ColdFormedSteel;
    case BarMaterialIcon::Timber: return SectionFamily::Timber;
    case BarMaterialIcon::ExtrudedAluminium:
    case BarMaterialIcon::SpecialAluminium: return SectionFamily::Aluminium;
    case BarMaterialIcon::Concrete:
    case BarMaterialIcon::Generic: return std::nullopt;
    }
    return std::nullopt;
}

MaterialKind materialKindOf(BarMaterialIcon icon) {
    switch (icon) {
    case BarMaterialIcon::RolledSteel:
    case BarMaterialIcon::RolledSteelFamily2:
    case BarMaterialIcon::ColdFormedSteel: return MaterialKind::Steel;
    case BarMaterialIcon::Timber: return MaterialKind::Timber;
    case BarMaterialIcon::ExtrudedAluminium:
    case BarMaterialIcon::SpecialAluminium: return MaterialKind::Aluminium;
    case BarMaterialIcon::Concrete: return MaterialKind::Concrete;
    case BarMaterialIcon::Generic: return MaterialKind::Generic;
    }
    return MaterialKind::Generic;
}

std::vector<ShapeKind> shapesFor(BarMaterialIcon icon) {
    if (icon == BarMaterialIcon::Generic) return {ShapeKind::Generic};
    if (icon == BarMaterialIcon::Concrete) return shapesOfFamily(SectionFamily::Concrete);
    const auto family = familyOfIcon(icon);
    std::vector<ShapeKind> out = family ? shapesOfFamily(*family) : std::vector<ShapeKind>{};
    if (icon == BarMaterialIcon::RolledSteel) out.erase(std::remove(out.begin(), out.end(), ShapeKind::ISloped), out.end());
    return out;
}

std::vector<Arrangement> arrangementsFor(BarMaterialIcon icon, ShapeKind shape) {
    switch (icon) {
    case BarMaterialIcon::RolledSteel:
        if (isIShape(shape))
            return {Arrangement::Simple,         Arrangement::SimpleWithHaunches, Arrangement::WithConcreteSlab,
                    Arrangement::DoubleBoxWelded, Arrangement::DoubleBoxBattened,  Arrangement::DoubleBoxGeneric,
                    Arrangement::HalfProfile,     Arrangement::SideFlatPlates,     Arrangement::Boyd};
        return {Arrangement::Simple};
    case BarMaterialIcon::RolledSteelFamily2: return {Arrangement::Simple, Arrangement::WithConcreteSlab};
    case BarMaterialIcon::ColdFormedSteel:
        return {Arrangement::Simple,          Arrangement::DoubleBoxWelded, Arrangement::DoubleBoxBattened,
                Arrangement::DoubleBoxGeneric, Arrangement::DoubleIWelded,   Arrangement::DoubleIBattened,
                Arrangement::DoubleIGeneric};
    default: return {Arrangement::Simple};
    }
}

bool stiffenersAllowed(BarMaterialIcon icon, Arrangement arrangement) {
    const auto offered = arrangementsFor(icon, ShapeKind::IParallel);
    return icon == BarMaterialIcon::RolledSteelFamily2 &&
           std::find(offered.begin(), offered.end(), arrangement) != offered.end();
}

MaterialData defaultMaterialData(BarMaterialIcon icon) {
    MaterialData m;
    switch (icon) {
    case BarMaterialIcon::RolledSteel:
    case BarMaterialIcon::RolledSteelFamily2:
    case BarMaterialIcon::ColdFormedSteel: m = {210000.0, 0.3, 1.2e-5, 78.5}; break;
    case BarMaterialIcon::ExtrudedAluminium:
    case BarMaterialIcon::SpecialAluminium: m = {70000.0, 0.3, 2.3e-5, 27.0}; break;
    case BarMaterialIcon::Concrete: m = {30000.0, 0.2, 1.0e-5, 25.0}; break;
    case BarMaterialIcon::Timber: m = {11000.0, 0.3, 5.0e-6, 4.2}; break;
    case BarMaterialIcon::Generic: break;  // dialog defaults
    }
    return m;
}

namespace {
struct TimberClass {
    const char* name;
    double e0MeanMPa;
    double meanDensityKgM3;
};
// EN 338 softwood classes (E0,mean and mean density): NOT MEASURED in the original (TK-063).
constexpr TimberClass kSoftwood[] = {{"C14", 7000, 350},  {"C16", 8000, 370},  {"C18", 9000, 380},  {"C20", 9500, 390},
                                     {"C22", 10000, 410}, {"C24", 11000, 420}, {"C27", 11500, 450}, {"C30", 12000, 460},
                                     {"C35", 13000, 480}, {"C40", 14000, 500}, {"C45", 15000, 520}, {"C50", 16000, 550}};
struct ConcreteGrade {
    const char* name;
    double ecmMPa;
};
// EC2 table 3.1: Ecm of the strength classes. NOT MEASURED in the original (TK-063).
constexpr ConcreteGrade kConcrete[] = {{"C12/15", 27000}, {"C16/20", 29000}, {"C20/25", 30000}, {"C25/30", 31000}, {"C30/37", 33000},
                                       {"C35/45", 34000}, {"C40/50", 35000}, {"C45/55", 36000}, {"C50/60", 37000}};
}  // namespace

MaterialData concreteMaterialData(const std::string& grade) {
    MaterialData m = defaultMaterialData(BarMaterialIcon::Concrete);
    for (const ConcreteGrade& g : kConcrete)
        if (grade == g.name) m.elasticModulusMPa = g.ecmMPa;
    return m;
}

MaterialData timberMaterialData(const std::string& timberMaterial, const std::string& strengthClass) {
    MaterialData m = defaultMaterialData(BarMaterialIcon::Timber);
    if (timberMaterial != "Aserrada, procedente de coníferas o chopos") return m;
    for (const TimberClass& c : kSoftwood)
        if (strengthClass == c.name) {
            m.elasticModulusMPa = c.e0MeanMPa;
            m.densityKnM3 = c.meanDensityKgM3 * 9.80665 / 1000.0;
        }
    return m;
}

double timberShearModulusMPa(const std::string& strengthClass) {
    for (const TimberClass& c : kSoftwood)
        if (strengthClass == c.name) return c.e0MeanMPa / 16.0;
    return 0.0;
}

double shearModulusMPa(const MaterialData& m) { return m.elasticModulusMPa / (2.0 * (1.0 + m.poissonRatio)); }

BarDescription defaultDescription(BarMaterialIcon icon) {
    BarDescription d;
    d.materialIcon = icon;
    d.material = defaultMaterialData(icon);
    d.seriesRef.clear();
    d.profileRef.clear();
    const auto shapes = shapesFor(icon);
    d.shape = shapes.empty() ? ShapeKind::IParallel : shapes.front();
    switch (icon) {
    case BarMaterialIcon::RolledSteel:
        d.seriesRef = "IPE";
        d.profileRef = "IPE 80";
        break;
    case BarMaterialIcon::Concrete:
        d.inlineSection = defaultConcreteSection();
        d.shape = d.inlineSection->kind;
        break;
    case BarMaterialIcon::Timber: {
        Section s;
        s.reference = "30x30";
        s.kind = ShapeKind::TimberRectangular;
        s.dims = {0.30, 0.30};
        d.inlineSection = s;
        d.shape = s.kind;
        break;
    }
    default: break;
    }
    return d;
}

const BarDescription* BarDescriptions::find(int id) const {
    const auto it = std::lower_bound(items.begin(), items.end(), id, [](const BarDescription& d, int v) { return d.id < v; });
    return it != items.end() && it->id == id ? &*it : nullptr;
}

const BarDescription* BarDescriptions::findBySection(int sectionId) const {
    if (sectionId == 0) return nullptr;
    for (const BarDescription& d : items)
        if (d.sectionId == sectionId) return &d;
    return nullptr;
}

std::optional<Section> resolveSection(const BarDescription& d, const ProfileLibrary& library) {
    if (d.materialIcon == BarMaterialIcon::Generic) {
        const GenericSectionData& g = d.generic;
        Section s;
        s.reference = "Genérico";
        s.kind = ShapeKind::Generic;
        s.dims = {g.widthLeftM, g.widthRightM, g.depthBottomM, g.depthTopM};
        s.tabulated.area = g.areaM2;
        s.tabulated.iy = g.inertiaYM4;
        s.tabulated.iz = g.inertiaZM4;
        s.tabulated.it = g.torsionM4;
        s.tabulated.avy = g.shearAreaYM2;
        s.tabulated.avz = g.shearAreaZM2;
        return s;
    }
    const bool inlineOnly = d.materialIcon == BarMaterialIcon::Concrete ||
                            (d.materialIcon == BarMaterialIcon::Timber && d.seriesRef.empty());
    if (inlineOnly) return d.inlineSection;
    const ProfileSeries* series = library.find(d.seriesRef);
    if (!series) return std::nullopt;
    const Section* profile = series->findProfile(d.profileRef);
    if (!profile) return std::nullopt;
    Section out = *profile;
    if (out.kind == ShapeKind::TimberRectangularVariable && out.dims.size() == 3) {
        out.dims[1] = d.depthStartM;  // "Canto inicial" / "Canto final" of the bar
        out.dims[2] = d.depthEndM;
    }
    return out;
}

std::string validateDescription(const BarDescription& d, const ProfileLibrary& library) {
    const auto offered = arrangementsFor(d.materialIcon, d.shape);
    if (std::find(offered.begin(), offered.end(), d.arrangement) == offered.end())
        return "La disposición «" + std::string(arrangementNameEs(d.arrangement)) + "» no está disponible para esta sección.";
    if (d.stiffeners.present) {
        if (!stiffenersAllowed(d.materialIcon, d.arrangement)) return "Esta sección no admite rigidizadores de alma.";
        if (!finitePositive(d.stiffeners.spacingM) || !finitePositive(d.stiffeners.thicknessM))
            return "La separación y el espesor de los rigidizadores deben ser mayores que cero.";
    }
    const MaterialData& m = d.material;
    if (!finitePositive(m.elasticModulusMPa) || !std::isfinite(m.poissonRatio) || m.poissonRatio <= -1.0 || m.poissonRatio >= 0.5 ||
        !std::isfinite(m.thermalExpansion) || m.thermalExpansion < 0.0 || !finitePositive(m.densityKnM3))
        return "Los valores del material no son válidos.";

    if (d.materialIcon == BarMaterialIcon::Generic) {
        const auto s = resolveSection(d, library);
        const std::string problem = validateSection(*s);
        if (!problem.empty()) return problem;
        const GenericSectionData& g = d.generic;
        if (!finitePositive(g.torsionM4)) return "La inercia a torsión debe ser mayor que cero.";
        if (!finitePositive(g.shearAreaYM2) || !finitePositive(g.shearAreaZM2))
            return "Los valores de las áreas de cortante deben ser mayores que cero.";
        return {};
    }
    const auto section = resolveSection(d, library);
    const bool inlineOnly = d.materialIcon == BarMaterialIcon::Concrete ||
                            (d.materialIcon == BarMaterialIcon::Timber && d.seriesRef.empty());
    if (inlineOnly) {
        if (!section) return "Debe definir la sección.";
        const auto family = shapeFamily(section->kind);
        const SectionFamily wanted = d.materialIcon == BarMaterialIcon::Concrete ? SectionFamily::Concrete : SectionFamily::Timber;
        if (!family || *family != wanted) return "La forma de la sección no corresponde al material.";
        return validateSection(*section);
    }
    if (d.seriesRef.empty() || d.profileRef.empty()) return "Debe seleccionar una serie y un perfil.";
    const ProfileSeries* series = library.find(d.seriesRef);
    if (!series) return "La serie «" + d.seriesRef + "» no existe en la biblioteca de perfiles.";
    const auto family = familyOfIcon(d.materialIcon);
    if (family && series->family != *family) return "La serie «" + d.seriesRef + "» no corresponde al material seleccionado.";
    if (!series->findProfile(d.profileRef)) return "El perfil «" + d.profileRef + "» no existe en la serie «" + d.seriesRef + "».";
    if (shapeGroup(series->kind) != shapeGroup(d.shape)) return "La forma del perfil no corresponde a la forma seleccionada.";
    const std::string problem = validateSection(*section);
    if (!problem.empty()) return problem;
    // Built-up, cold-formed and aluminium profiles are in the library but their properties cannot be calculated yet (TK-052).
    if (isCatalogueOnly(section->kind)) return "Las propiedades mecánicas de los perfiles de esta serie aún no se pueden calcular.";
    return {};
}

bool sameDescription(const BarDescription& a, const BarDescription& b) {
    BarDescription x = a, y = b;
    x.id = y.id = 0;
    x.sectionId = y.sectionId = 0;
    x.materialId = y.materialId = 0;
    return x == y;
}

std::string descriptionText(const BarDescription& d, const std::string& hotRolledSteelGrade) {
    if (d.materialIcon == BarMaterialIcon::Generic) return "Genérico";
    std::string name = d.profileRef;
    if (name.empty() && d.inlineSection) name = d.inlineSection->reference;
    if (d.materialIcon == BarMaterialIcon::RolledSteel && !hotRolledSteelGrade.empty() && !name.empty())
        return name + " (" + hotRolledSteelGrade + ")";
    return name;
}

namespace {
std::string compactNumber(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.10g", value);
    return buffer;
}
}  // namespace

std::string barLabelText(const BarDescription& d) {
    if (d.materialIcon == BarMaterialIcon::Generic) {
        constexpr double g = 9.80665;
        char buffer[48];
        std::snprintf(buffer, sizeof buffer, "Gen.(%.1f kg/m)", d.generic.areaM2 * d.material.densityKnM3 * 1000.0 / g);
        return buffer;
    }
    if (d.materialIcon == BarMaterialIcon::Concrete && d.inlineSection && d.inlineSection->kind == ShapeKind::ConcreteRectangular &&
        d.inlineSection->dims.size() == 2)
        return compactNumber(d.inlineSection->dims[0] * 100.0) + " cm x " + compactNumber(d.inlineSection->dims[1] * 100.0) + " cm";
    if (!d.profileRef.empty()) return d.profileRef;
    return d.inlineSection ? d.inlineSection->reference : std::string();
}

}  // namespace cc::core

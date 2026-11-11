#include "core/fem/frame_builder.h"

#include <cmath>
#include <set>

#include "core/bar_description.h"
#include "core/deflection_group.h"
#include "core/section_properties.h"

namespace cc::fem {

namespace {

using core::Point3;

Vec3 vec(const Point3& p) { return {p.x, p.y, p.z}; }
double dot(const Point3& a, const Point3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/// Averaged section of a bar (see the header).
SectionData sectionOf(const core::Project& project, const core::CrossSection& cs) {
    SectionData s{cs.areaM2, cs.inertiaYM4, cs.inertiaZM4, cs.torsionM4, cs.shearAreaYM2, cs.shearAreaZM2, 0.0, 0.0};
    const core::BarDescription* description = project.barDescriptions.findBySection(cs.id);
    if (!description) return s;
    const std::optional<core::Section> section = core::resolveSection(*description, project.profileLibrary);
    if (!section) return s;
    const auto p0 = core::propertiesAt(*section, 0.0), p1 = core::propertiesAt(*section, 1.0);
    if (!p0 || !p1) return s;
    const auto avg = [](double a, double b) { return 0.5 * (a + b); };
    s.area = avg(p0->area, p1->area);
    s.iy = avg(p0->iy, p1->iy);
    s.iz = avg(p0->iz, p1->iz);
    s.torsion = avg(p0->it, p1->it);
    s.avy = avg(p0->avy, p1->avy);
    s.avz = avg(p0->avz, p1->avz);
    s.depthY = avg(p0->yNeg + p0->yPos, p1->yNeg + p1->yPos);
    s.depthZ = avg(p0->zNeg + p0->zPos, p1->zNeg + p1->zPos);
    return s;
}

EndRelease releaseOf(const core::EndPlaneFixity& fixity, double flexuralStiffness4EIoverL) {
    EndRelease r;
    if (fixity.kind == core::EndFixityKind::Rotational) {
        if (fixity.stiffnessKnMPerRad <= 0.0) r.kind = EndRelease::Kind::Free;
        else { r.kind = EndRelease::Kind::Spring; r.stiffness = fixity.stiffnessKnMPerRad; }
        return r;
    }
    const double c = fixity.coefficient;
    if (c >= 1.0 - 1e-9) return r;  // rigid
    if (c <= 1e-9) { r.kind = EndRelease::Kind::Free; return r; }
    r.kind = EndRelease::Kind::Spring;
    r.stiffness = c / (1.0 - c) * flexuralStiffness4EIoverL;  // [N] law of the original not verified (TK-105)
    return r;
}

}  // namespace

std::string undescribedBarsMessage() { return "Existen barras sin describir."; }
std::string unconnectedNodeMessage() { return "Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina."; }

FrameBuild buildFrameModel(const core::Project& project) {
    FrameBuild out;
    const core::StructuralModel& sm = project.structure;

    // --- nodes used by bars ---
    std::set<int> barNodes, shellNodes;
    for (const core::Bar& b : sm.bars) { barNodes.insert(b.startNode); barNodes.insert(b.endNode); }
    for (const core::Shell& s : sm.shells) shellNodes.insert(s.nodeIds.begin(), s.nodeIds.end());
    bool unconnectedWarned = false;
    for (const core::Node& n : sm.nodes) {
        if (!barNodes.count(n.id)) {
            if (!shellNodes.count(n.id) && !unconnectedWarned) {
                out.warnings.push_back(unconnectedNodeMessage());
                unconnectedWarned = true;
            }
            continue;
        }
        FemNode fn;
        fn.id = n.id;
        fn.position = vec(n.position);
        if (const core::ExternalSupport* ext = core::externalSupportOf(sm, project.nodeSupports, n.id)) {
            const core::Restraint r = core::restraintOf(*ext);
            for (int k = 0; k < 6; ++k) fn.fixed[k] = r.fixed[k] && !r.elastic[k];
            bool elastic = false;
            for (int k = 0; k < 6; ++k) elastic = elastic || (r.fixed[k] && r.elastic[k]);
            if (elastic) out.unsupported.push_back("Nudo " + std::to_string(n.id) + ": apoyo elástico (pendiente, T-P8-03).");
            if (r.oblique != core::Restraint::Oblique::None)
                out.unsupported.push_back("Nudo " + std::to_string(n.id) + ": apoyo con dirección no alineada con los ejes globales (pendiente, T-P8-03).");
        }
        out.model.nodes.push_back(fn);
    }

    // --- data of later tasks ---
    for (const auto& [nodeId, link] : project.nodeSupports.internal)
        if (link.type != core::InternalLinkType::Fixed && barNodes.count(nodeId))
            out.unsupported.push_back("Nudo " + std::to_string(nodeId) + ": vinculación interior (pendiente, T-P8-03).");
    if (!project.ligatures.empty()) out.unsupported.push_back("Ligaduras de nudos (pendiente, T-P8-03).");
    if (!project.diaphragms.empty()) out.unsupported.push_back("Diafragmas rígidos (pendiente, T-P8-03).");
    if (!sm.shells.empty()) out.unsupported.push_back("Láminas (pendiente, T-P8-02).");

    // --- bars ---
    struct BarInfo { bool inverted = false; double length = 0.0; core::BarFrame frame; double gamma = 0.0; double depthY = 0.0, depthZ = 0.0; };
    std::map<int, BarInfo> info;
    bool undescribed = false;
    for (const core::Bar& b : sm.bars) {
        const core::Material* mat = sm.findMaterial(b.materialId);
        const core::CrossSection* cs = sm.findSection(b.sectionId);
        const std::optional<core::BarFrame> frame = core::barLocalAxes(sm, b.id);
        if (!mat || !cs) { undescribed = true; continue; }
        if (!frame) { out.errors.push_back("La barra " + std::to_string(b.id) + " tiene longitud nula."); continue; }
        FemBar fb;
        fb.id = b.id;
        fb.nodeI = core::barXOrigin(b);
        fb.nodeJ = fb.nodeI == b.startNode ? b.endNode : b.startNode;
        fb.e = mat->elasticModulusMPa * 1000.0;
        fb.g = mat->shearModulusMPa() * 1000.0;
        fb.alpha = mat->thermalExpansion;
        fb.section = sectionOf(project, *cs);
        fb.ex = vec(frame->x);
        fb.ey = vec(frame->y);
        fb.ez = vec(frame->z);
        const double length = frame->length;
        const core::BarEndsRecord record = core::barEndsOf(project.barEnds, b.id);
        for (int end = 0; end < 2; ++end) {
            const int node = end == 0 ? fb.nodeI : fb.nodeJ;
            const core::BarEndFixity& fix = record.ends[node == b.startNode ? core::kBarOrigin : core::kBarFinal];
            const double kz = 4.0 * fb.e * fb.section.iz / length, ky = 4.0 * fb.e * fb.section.iy / length;
            fb.releases[end][0] = releaseOf(fix.planes[core::kPlaneYZ], 4.0 * fb.g * fb.section.torsion / length);
            fb.releases[end][1] = releaseOf(fix.planes[core::kPlaneXZ], ky);
            fb.releases[end][2] = releaseOf(fix.planes[core::kPlaneXY], kz);
            if (fix.pinned) fb.releases[end][1] = fb.releases[end][2] = EndRelease{EndRelease::Kind::Free, 0.0};
        }
        out.model.bars.push_back(fb);
        info[b.id] = BarInfo{b.xAxisInverted, length, *frame, mat->densityKnM3 * fb.section.area, fb.section.depthY, fb.section.depthZ};
    }
    if (undescribed) out.errors.insert(out.errors.begin(), undescribedBarsMessage());

    // --- load cases ---
    for (const core::LoadCase& lc : project.loadCases.cases) {
        LoadCaseInput in;
        in.id = lc.id;
        in.name = core::loadCaseLabel(lc);
        for (const core::NodalLoad& nl : project.nodalLoads.items) {
            if (nl.loadCaseId != lc.id || !barNodes.count(nl.nodeId)) continue;
            const Point3 f = core::nodalLoadForce(nl);
            in.nodal.push_back(FemNodalLoad{nl.nodeId, {f.x, f.y, f.z, 0.0, 0.0, 0.0}});
        }
        for (const core::BarLoad& bl : project.barLoads.items) {
            if (bl.loadCaseId != lc.id) continue;
            const auto it = info.find(bl.barId);
            if (it == info.end()) continue;
            const BarInfo& bi = it->second;
            const double length = bi.length;
            core::BarLoadExtent ext = core::barLoadExtent(bl, length);
            if (bi.inverted) {
                ext = core::BarLoadExtent{length - ext.end, length - ext.start, ext.valueEnd, ext.valueStart};
            }
            const Point3 dir = core::barLoadDirection(bl, bi.frame);
            FemBarLoad fl;
            fl.bar = bl.barId;
            fl.global = true;
            fl.x1 = ext.start;
            fl.x2 = ext.end;
            const auto scaled = [&](double v) { return Vec3{v * dir.x, v * dir.y, v * dir.z}; };
            switch (bl.type) {
                case core::BarLoadType::Point:
                    fl.kind = BarLoadKind::PointForce;
                    fl.w1 = scaled(ext.valueStart);
                    break;
                case core::BarLoadType::PointMoment:
                    fl.kind = BarLoadKind::PointMoment;
                    fl.w1 = scaled(ext.valueStart);
                    break;
                case core::BarLoadType::Uniform:
                case core::BarLoadType::Strip:
                case core::BarLoadType::TriangularLeft:
                case core::BarLoadType::TriangularRight:
                case core::BarLoadType::Trapezoidal:
                    fl.kind = BarLoadKind::Distributed;
                    fl.w1 = scaled(ext.valueStart);
                    fl.w2 = scaled(ext.valueEnd);
                    break;
                case core::BarLoadType::UniformTemperature:
                    fl.kind = BarLoadKind::Temperature;
                    fl.global = false;
                    fl.uniformTemperature = bl.value1;
                    break;
                case core::BarLoadType::VariableTemperature: {
                    fl.kind = BarLoadKind::Temperature;
                    fl.global = false;
                    const double grad = bl.value1 - bl.value2;
                    fl.uniformTemperature = 0.5 * (bl.value1 + bl.value2);
                    fl.gradientY = grad * dot(dir, bi.frame.y);
                    fl.gradientZ = grad * dot(dir, bi.frame.z);
                    if ((std::fabs(fl.gradientY) > 1e-12 && bi.depthY <= 0.0) || (std::fabs(fl.gradientZ) > 1e-12 && bi.depthZ <= 0.0))
                        out.warnings.push_back("Barra " + std::to_string(bl.barId) +
                                               ": canto de la sección desconocido; no se considera el gradiente de la carga de temperatura variable.");
                    break;
                }
            }
            in.bars.push_back(fl);
        }
        if (lc.id == 0) {
            for (const FemBar& fb : out.model.bars) {
                if (!core::barSelfWeightActive(project.barLoads, fb.id)) continue;
                const BarInfo& bi = info[fb.id];
                FemBarLoad fl;
                fl.bar = fb.id;
                fl.kind = BarLoadKind::Distributed;
                fl.global = true;
                fl.x1 = 0.0;
                fl.x2 = bi.length;
                fl.w1 = fl.w2 = Vec3{0.0, 0.0, -bi.gamma};
                in.bars.push_back(fl);
            }
        }
        for (const auto& [nodeId, ext] : project.nodeSupports.external) {
            if (!barNodes.count(nodeId)) continue;
            for (const core::PrescribedDisplacement& pd : core::effectivePrescribed(ext)) {
                if (pd.loadCaseId != lc.id) continue;
                Dof6 v{};
                for (int k = 0; k < 6; ++k) v[k] = pd.values[k] / 1000.0;
                in.prescribed[nodeId] = v;
            }
        }
        out.cases.push_back(std::move(in));
    }
    return out;
}

}  // namespace cc::fem

#include "core/fem/frame_builder.h"

#include <cmath>
#include <set>

#include "core/bar_description.h"
#include "core/deflection_group.h"
#include "core/section_properties.h"
#include "core/shell_edit.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"
#include "core/fem/shell_analysis.h"

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

/// Diagonal rotational stiffness of the bar element at one end, the base of the partial fixity law of the original [V, TK-105]:
/// k_ii = (4 + phi) E I / ((1 + phi) L) with phi = 12 E I / (G Av L^2) (phi = 0 without shear area). For torsion (plane yz) it is G J / L [N].
double flexuralDiagonal(double e, double g, double inertia, double shearArea, double length) {
    const double phi = shearArea > 0.0 ? 12.0 * e * inertia / (g * shearArea * length * length) : 0.0;
    return (4.0 + phi) * e * inertia / ((1.0 + phi) * length);
}

EndRelease releaseOf(const core::EndPlaneFixity& fixity, double diagonalStiffness) {
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
    r.stiffness = c / (1.0 - c) * diagonalStiffness;  // K = c / (1 - c) k_ii, measured with two coefficients (TK-105)
    return r;
}

/// Elastic data of a shell material (kN/m2). Concrete: the grade of the shell or of the general data (Eurocode 2 table, nu 0.2, alpha 1e-5);
/// steel and aluminium: the default values of the bars [N for shells]; generic: the typed values.
void shellMaterial(const core::Project& project, const core::ShellMaterial& m, FemShell& out) {
    core::MaterialData d;
    switch (m.kind) {
        case core::ShellMaterialKind::Concrete:
            d = core::concreteMaterialData(m.useGeneralData || m.grade.empty() ? project.profiles.concrete : m.grade);
            break;
        case core::ShellMaterialKind::RolledSteel:
        case core::ShellMaterialKind::ColdFormedSteel: d = core::defaultMaterialData(core::BarMaterialIcon::RolledSteel); break;
        case core::ShellMaterialKind::Aluminium: d = core::defaultMaterialData(core::BarMaterialIcon::ExtrudedAluminium); break;
        case core::ShellMaterialKind::GenericIsotropic:
            d = {m.isoElasticModulusMPa, m.isoPoissonRatio, m.isoThermalExpansion, m.isoDensityKnM3};
            break;
        case core::ShellMaterialKind::GenericOrthotropic:
            out.orthotropic = true;
            out.e1 = m.orthoElasticModulusXMPa * 1000.0;
            out.e2 = m.orthoElasticModulusYMPa * 1000.0;
            out.nu12 = m.orthoPoissonRatioXY;
            out.g12 = m.orthoShearModulusXYMPa * 1000.0;
            out.g13 = m.orthoShearModulusXZMPa * 1000.0;
            out.g23 = m.orthoShearModulusYZMPa * 1000.0;
            out.alpha1 = m.orthoThermalExpansionX;
            out.alpha2 = m.orthoThermalExpansionY;
            return;
    }
    out.e = d.elasticModulusMPa * 1000.0;
    out.nu = d.poissonRatio;
    out.alpha1 = out.alpha2 = d.thermalExpansion;
}

}  // namespace

std::string undescribedBarsMessage() { return "Existen barras sin describir."; }
std::string shearAreaRangeMessage(const char* name) {
    return std::string("El valor de 'Área de cortante (") + name + ")' debe estar entre 0.01 y 1000000.00.";
}
std::string unconnectedNodeMessage() { return "Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina."; }

FrameBuild buildFrameModel(const core::Project& project, const FrameBuildOptions& options) {
    FrameBuild out;
    const core::StructuralModel& sm = project.structure;

    // --- nodes used by bars ---
    std::set<int> barNodes, shellNodes;
    for (const core::Bar& b : sm.bars) { barNodes.insert(b.startNode); barNodes.insert(b.endNode); }
    for (const core::Shell& s : sm.shells) shellNodes.insert(s.nodeIds.begin(), s.nodeIds.end());
    bool unconnectedWarned = false;
    for (const core::Node& n : sm.nodes) {
        if (!barNodes.count(n.id) && !shellNodes.count(n.id)) {
            if (!unconnectedWarned) {
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
            for (int k = 0; k < 6; ++k) {
                fn.fixed[k] = r.fixed[k] && !r.elastic[k];
                if (r.fixed[k] && r.elastic[k]) {
                    // Elastic support: spring K (kN/m, kN.m/rad) to the ground with its "Sentido" [V, solver_diafragmas_ligaduras_apoyos.md 10.1/10.4].
                    fn.spring[k] = ext->elasticConstants[k];
                    fn.springSense[k] = ext->senses[k] == core::SupportSense::Positive   ? SpringSense::Positive
                                        : ext->senses[k] == core::SupportSense::Negative ? SpringSense::Negative
                                                                                         : SpringSense::Both;
                }
            }
            if (r.oblique != core::Restraint::Oblique::None) {
                // Oblique line / plane: exact relation on the translations (always rigid [V 10.4]); rotations follow `fixed`.
                fn.oblique = r.oblique == core::Restraint::Oblique::LineFree ? ObliqueSupport::Line : ObliqueSupport::Plane;
                fn.obliqueDirection = vec(r.direction);
            }
        }
        out.model.nodes.push_back(fn);
    }
    const auto inModel = [&](int nodeId) { return barNodes.count(nodeId) > 0 || shellNodes.count(nodeId) > 0; };

    // --- internal links of nodes (TK-106) ---
    std::map<int, double> partialNodes;  // node id -> Coef.Emp.Parc. (kN.m/rad)
    for (const auto& [nodeId, link] : project.nodeSupports.internal) {
        if (link.type == core::InternalLinkType::Fixed || !inModel(nodeId)) continue;
        if (link.type == core::InternalLinkType::PartialFixity && !shellNodes.count(nodeId)) {
            partialNodes[nodeId] = link.partialCoefficient;
            continue;
        }
        const char* what = link.type == core::InternalLinkType::PartialFixity ? "empotramiento parcial en un nudo con láminas"
                           : link.type == core::InternalLinkType::Pinned      ? "nudo articulado"
                                                                              : "nudo genérico";
        out.unsupported.push_back("Nudo " + std::to_string(nodeId) + ": vinculación interior, " + what + " (pendiente, sin medir en el original).");
    }

    // --- ligatures (equalDOF) [V 10.3]: the first node of the tie present in the model is the master ---
    for (int dir = 0; dir < 3; ++dir)
        for (const core::Ligature& tie : project.ligatures.dirs[dir]) {
            int master = -1;
            for (int id : tie.nodeIds) {
                if (!inModel(id)) continue;
                if (master < 0) { master = id; continue; }
                FemEqualDof e;
                e.master = master;
                e.slave = id;
                e.dofs[dir] = true;
                out.model.equalDofs.push_back(e);
            }
        }

    // --- rigid diaphragms [REA 2, V 10.2] ---
    for (const core::Diaphragm& d : project.diaphragms.items) {
        FemDiaphragm fd;
        fd.id = d.id;
        for (int id : d.nodeIds)
            if (inModel(id)) fd.nodes.push_back(id);
        if (!d.shellIds.empty())
            out.unsupported.push_back("Diafragma " + d.reference + ": láminas en el diafragma (pendiente, sin medir en el original).");
        if (!fd.nodes.empty()) out.model.diaphragms.push_back(fd);
    }

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
        // The original always applies shear deformation and does not admit a shear area of 0 (range 0.01..1000000 cm2) [V, TK-105g]: report it
        // instead of silently falling back to Euler-Bernoulli (use 1e6 cm2 for pure bending).
        {
            const SectionData probe = sectionOf(project, *cs);
            const auto outside = [](double m2) { return !(m2 >= kMinShearAreaM2 * (1.0 - 1e-9) && m2 <= kMaxShearAreaM2 * (1.0 + 1e-9)); };
            if (outside(probe.avy)) { out.errors.push_back("Barra " + std::to_string(b.id) + ": " + shearAreaRangeMessage("Avy")); continue; }
            if (outside(probe.avz)) { out.errors.push_back("Barra " + std::to_string(b.id) + ": " + shearAreaRangeMessage("Avz")); continue; }
        }
        FemBar fb;
        fb.id = b.id;
        // Origin of the local x axis: the node from which x points (a vertical bar always has x = +Z, whatever its definition [V, TK-105f]).
        {
            const core::Node* a = sm.findNode(b.startNode);
            const core::Node* c = sm.findNode(b.endNode);
            const bool forward = !a || !c || dot(frame->x, Point3{c->position.x - a->position.x, c->position.y - a->position.y,
                                                                  c->position.z - a->position.z}) > 0.0;
            fb.nodeI = forward ? b.startNode : b.endNode;
        }
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
            const double kz = flexuralDiagonal(fb.e, fb.g, fb.section.iz, fb.section.avy, length);   // plane xy: Iz, Avy
            const double ky = flexuralDiagonal(fb.e, fb.g, fb.section.iy, fb.section.avz, length);   // plane xz: Iy, Avz
            fb.releases[end][0] = releaseOf(fix.planes[core::kPlaneYZ], fb.g * fb.section.torsion / length);
            fb.releases[end][1] = releaseOf(fix.planes[core::kPlaneXZ], ky);
            fb.releases[end][2] = releaseOf(fix.planes[core::kPlaneXY], kz);
            if (fix.pinned) fb.releases[end][1] = fb.releases[end][2] = EndRelease{EndRelease::Kind::Free, 0.0};
            // «Nudo empotrado con coeficiente de empotramiento parcial para todas las piezas» (TK-106): every bar end joins the node through a
            // rotational spring c (kN.m/rad, used as typed), in series with its own release. Two pieces give c / 2 between them [V 10.5];
            // the law with three or more pieces, the torsion (left rigid) and the plane xy (Gz, given the same c) are [N].
            if (const auto pn = partialNodes.find(node); pn != partialNodes.end())
                for (int k = 1; k <= 2; ++k) {
                    EndRelease& rel = fb.releases[end][k];
                    const double c = pn->second;
                    if (rel.kind == EndRelease::Kind::Free) continue;
                    if (!(c > 0.0)) { rel = EndRelease{EndRelease::Kind::Free, 0.0}; continue; }
                    rel.stiffness = rel.kind == EndRelease::Kind::Spring ? rel.stiffness * c / (rel.stiffness + c) : c;
                    rel.kind = EndRelease::Kind::Spring;
                }
        }
        out.model.bars.push_back(fb);
        info[b.id] = BarInfo{fb.nodeI != b.startNode, length, *frame, mat->densityKnM3 * fb.section.area, fb.section.depthY, fb.section.depthZ};
    }
    if (undescribed) out.errors.insert(out.errors.begin(), undescribedBarsMessage());

    // --- shells (T-P8-02) ---
    std::map<int, std::vector<core::Point2>> shellContours;
    for (const core::Shell& sh : sm.shells) {
        const auto axes = core::shellLocalAxes(sm, sh);
        if (!axes || sh.nodeIds.size() < 3) { out.errors.push_back("La lámina " + std::to_string(sh.id) + " no tiene un contorno válido."); continue; }
        const std::string tag = "Lámina " + std::to_string(sh.id) + ": ";
        FemShell fs;
        fs.id = sh.id;
        fs.nodes = sh.nodeIds;
        fs.ex = vec(axes->x);
        fs.ey = vec(axes->y);
        fs.ez = vec(axes->z);
        fs.thickness = sh.thicknessM;
        shellMaterial(project, sh.material, fs);
        const core::ShellRecord rec = core::shellRecordOf(project.shellProps, sh.id);
        fs.maxSide = rec.maxTriangleSideM;
        for (int k = 0; k < core::kStiffnessFactorCount; ++k) fs.factors[k] = rec.stiffness.value(k);
        if (sh.ballastEnabled) {
            fs.ballast = sh.ballastKnM3;
            if (sh.ballastBehaviour == core::BallastBehaviour::CompressionOnly)
                out.unsupported.push_back(tag + "balasto «Sólo compresión» (cálculo no lineal, pendiente).");
        }
        if (sh.layout.considerEccentricity && std::fabs(sh.layout.centralPlaneOffsetM) > 1e-9)
            out.unsupported.push_back(tag + "excentricidad del plano central (pendiente).");
        for (const core::ShellEdgeKey& key : core::shellEdges(sm, sh.id)) {
            std::array<bool, 6> mask{};
            const core::Restraint r = core::restraintOf(core::edgeExteriorOf(project.shellProps, key));
            bool elastic = false;
            for (int k = 0; k < 6; ++k) {
                mask[k] = r.fixed[k] && !r.elastic[k];
                elastic = elastic || (r.fixed[k] && r.elastic[k]);
            }
            if (elastic) out.unsupported.push_back(tag + "vinculación exterior elástica de arista (pendiente, T-P8-03).");
            if (r.oblique != core::Restraint::Oblique::None)
                out.unsupported.push_back(tag + "vinculación exterior de arista no alineada con los ejes globales (pendiente, T-P8-03).");
            if (!core::isDefaultEdgeInterior(core::edgeInteriorOf(project.shellProps, key)))
                out.unsupported.push_back(tag + "vinculación interior de arista distinta de «Empotrada» (pendiente, T-P8-03).");
            fs.edgeFixed.push_back(mask);
        }
        out.model.shells.push_back(fs);
        shellContours[sh.id] = core::shellContour2D(sm, sh);
    }
    if (!out.model.shells.empty())
        for (const std::string& issue : meshShells(out.model).issues) out.unsupported.push_back(issue);

    // --- load cases ---
    for (const core::LoadCase& lc : project.loadCases.cases) {
        LoadCaseInput in;
        in.id = lc.id;
        in.name = core::loadCaseLabel(lc);
        for (const core::NodalLoad& nl : project.nodalLoads.items) {
            if (nl.loadCaseId != lc.id || (!barNodes.count(nl.nodeId) && !shellNodes.count(nl.nodeId))) continue;
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
                    // Measured [V]: T1 acts on the face opposite to the chosen direction, T2 on the face towards it
                    // (direction Z+: T1 on the -z face; T1 = 20, T2 = 0 lifts the tip of a cantilever by alpha (T1 - T2) L^2 / (2 h)).
                    const double grad = bl.value2 - bl.value1;  // T(face towards dir) - T(opposite face)
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
        for (const core::ShellLoad& sl : project.shellLoads.items) {
            if (sl.loadCaseId != lc.id || !shellContours.count(sl.shellId)) continue;
            FemShellLoad fl;
            fl.shell = sl.shellId;
            switch (sl.type) {
                case core::ShellLoadType::Uniform:
                case core::ShellLoadType::VariableX:
                case core::ShellLoadType::VariableY: {
                    fl.kind = ShellLoadKind::Pressure;
                    fl.global = sl.axes == core::ShellLoadAxes::Global;
                    fl.direction = {0.0, 0.0, sl.sense == core::ShellLoadSense::PlusZ ? 1.0 : -1.0};
                    fl.p0 = sl.value1;
                    if (sl.type != core::ShellLoadType::Uniform) {
                        // Linear along local x (y) from value1 at the smallest coordinate of the contour to value2 at the largest (shell_load.h).
                        const bool alongX = sl.type == core::ShellLoadType::VariableX;
                        double lo = 1e300, hi = -1e300;
                        for (const core::Point2& q : shellContours[sl.shellId]) {
                            lo = std::min(lo, alongX ? q.x : q.y);
                            hi = std::max(hi, alongX ? q.x : q.y);
                        }
                        const double slope = hi - lo > 1e-12 ? (sl.value2 - sl.value1) / (hi - lo) : 0.0;
                        fl.p0 = sl.value1 - slope * lo;
                        (alongX ? fl.px : fl.py) = slope;
                    }
                    break;
                }
                case core::ShellLoadType::UniformTemperature:
                    fl.kind = ShellLoadKind::Temperature;
                    fl.value = sl.value1;
                    break;
                case core::ShellLoadType::Shrinkage:
                    fl.kind = ShellLoadKind::Shrinkage;
                    fl.value = sl.value1;
                    break;
            }
            in.shellLoads.push_back(fl);
        }
        for (const FemDiaphragm& fd : out.model.diaphragms) {
            const core::Diaphragm* d = core::findDiaphragm(project.diaphragms, fd.id);
            if (!d) continue;
            for (const core::DiaphragmLoad& dl : d->loads)
                if (dl.loadCaseId == lc.id && (dl.fxKn != 0.0 || dl.fyKn != 0.0))
                    in.diaphragmLoads.push_back(FemDiaphragmLoad{fd.id, dl.fxKn, dl.fyKn, dl.xM, dl.yM});
        }
        if (lc.id == 0) {
            // Self weight of the shells: gamma * h per square metre of surface along global -Z, unless switched off [V, section 13.5].
            for (const core::Shell& sh : sm.shells) {
                if (!shellContours.count(sh.id) || !core::shellSelfWeightActive(project.shellLoads, sh.id)) continue;
                FemShellLoad fl;
                fl.shell = sh.id;
                fl.kind = ShellLoadKind::Pressure;
                fl.global = true;
                fl.direction = {0.0, 0.0, -1.0};
                fl.p0 = core::shellSelfWeightKnM2(sh);
                in.shellLoads.push_back(fl);
            }
            for (const FemBar& fb : out.model.bars) {
                if (options.honourSelfWeightSwitch && !core::barSelfWeightActive(project.barLoads, fb.id)) continue;
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
            if (!barNodes.count(nodeId) && !shellNodes.count(nodeId)) continue;
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

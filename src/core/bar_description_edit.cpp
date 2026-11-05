#include "core/bar_description_edit.h"

#include <algorithm>
#include <functional>
#include <set>

#include "core/piece_edit.h"

namespace cc::core {

namespace {

std::string materialName(const Project& project, BarMaterialIcon icon) {
    switch (icon) {
    case BarMaterialIcon::RolledSteel:
    case BarMaterialIcon::RolledSteelFamily2: return project.profiles.hotRolledSteel;
    case BarMaterialIcon::ColdFormedSteel: return project.profiles.coldFormedSteel;
    case BarMaterialIcon::Concrete: return project.profiles.concrete;
    case BarMaterialIcon::Generic: return "Genérico";
    default: return materialIconNameEs(icon);
    }
}

CrossSection makeCrossSection(const BarDescription& d, const Section& section, const SectionProperties& p) {
    CrossSection cs;
    cs.name = section.reference;
    cs.kind = materialKindOf(d.materialIcon);
    cs.areaM2 = p.area;
    cs.inertiaYM4 = p.iy;
    cs.inertiaZM4 = p.iz;
    cs.torsionM4 = p.it;
    cs.shearAreaYM2 = p.avy;
    cs.shearAreaZM2 = p.avz;
    return cs;
}

Material makeMaterial(const Project& project, const BarDescription& d) {
    Material m;
    m.name = materialName(project, d.materialIcon);
    m.kind = materialKindOf(d.materialIcon);
    // Concrete and timber take their values from the grade / class of the project (TK-063: standardised values, not measured in the original).
    MaterialData data = d.material;
    if (d.materialIcon == BarMaterialIcon::Concrete) data = concreteMaterialData(project.profiles.concrete);
    else if (d.materialIcon == BarMaterialIcon::Timber)
        data = timberMaterialData(project.profiles.timber.material, project.profiles.timber.strengthClass);
    m.elasticModulusMPa = data.elasticModulusMPa;
    m.poissonRatio = data.poissonRatio;
    m.densityKnM3 = data.densityKnM3;
    m.thermalExpansion = data.thermalExpansion;
    return m;
}

int findEqualMaterial(const StructuralModel& model, const Material& wanted) {
    for (const Material& m : model.materials) {
        Material a = m, b = wanted;
        a.id = b.id = 0;
        if (a == b) return m.id;
    }
    return 0;
}

bool descriptionUsesSection(const StructuralModel& model, int sectionId) {
    return std::any_of(model.bars.begin(), model.bars.end(), [&](const Bar& b) { return b.sectionId == sectionId; });
}

/// Removes the descriptions no bar uses, with the sections and materials they generated when nothing else uses them.
void prune(StructuralModel& model, BarDescriptions& descriptions) {
    std::vector<BarDescription> kept, dropped;
    for (const BarDescription& d : descriptions.items)
        (descriptionUsesSection(model, d.sectionId) ? kept : dropped).push_back(d);
    descriptions.items = std::move(kept);
    for (const BarDescription& d : dropped) {
        model.removeSection(d.sectionId);  // refused (and harmless) while a bar still uses it
        const bool sharedByDescription = std::any_of(descriptions.items.begin(), descriptions.items.end(),
                                                     [&](const BarDescription& o) { return o.materialId == d.materialId; });
        if (!sharedByDescription) model.removeMaterial(d.materialId);  // refused while a bar or shell uses it
    }
}

}  // namespace

// --- DescriptionCommand -------------------------------------------------------------------------------------------------

void DescriptionCommand::capture(const StructuralModel& edited, const BarDescriptions& editedDescriptions) {
    const StructuralModel& now = m_project.structure;
    m_descriptionsBefore = m_project.barDescriptions;
    m_descriptionsAfter = editedDescriptions;
    m_sectionsBefore = now.sections;
    m_sectionsAfter = edited.sections;
    m_materialsBefore = now.materials;
    m_materialsAfter = edited.materials;
    m_nextSectionBefore = now.nextSectionId;
    m_nextSectionAfter = edited.nextSectionId;
    m_nextMaterialBefore = now.nextMaterialId;
    m_nextMaterialAfter = edited.nextMaterialId;
    for (const Bar& after : edited.bars) {
        const Bar* before = now.findBar(after.id);
        if (before && !(*before == after)) {
            m_barsBefore.push_back(*before);
            m_barsAfter.push_back(after);
        }
    }
}

void DescriptionCommand::redo() {
    if (!valid()) return;
    StructuralModel& m = m_project.structure;
    m_project.barDescriptions = m_descriptionsAfter;
    m.sections = m_sectionsAfter;
    m.materials = m_materialsAfter;
    m.nextSectionId = std::max(m.nextSectionId, m_nextSectionAfter);
    m.nextMaterialId = std::max(m.nextMaterialId, m_nextMaterialAfter);
    for (const Bar& b : m_barsAfter)
        if (Bar* target = m.findBar(b.id)) *target = b;
}

void DescriptionCommand::undo() {
    if (!valid()) return;
    StructuralModel& m = m_project.structure;
    m_project.barDescriptions = m_descriptionsBefore;
    m.sections = m_sectionsBefore;
    m.materials = m_materialsBefore;
    m.nextSectionId = std::max(m.nextSectionId, m_nextSectionBefore);
    m.nextMaterialId = std::max(m.nextMaterialId, m_nextMaterialBefore);
    for (const Bar& b : m_barsBefore)
        if (Bar* target = m.findBar(b.id)) *target = b;
}

// --- DescribeBarsCommand ------------------------------------------------------------------------------------------------

DescribeBarsCommand::DescribeBarsCommand(Project& project, const std::vector<int>& barIds, BarDescription description)
    : DescriptionCommand(project) {
    StructuralModel work = project.structure;
    BarDescriptions descriptions = project.barDescriptions;

    std::vector<int> targets;
    for (int id : barIds)
        if (work.findBar(id) && std::find(targets.begin(), targets.end(), id) == targets.end()) targets.push_back(id);
    if (targets.empty()) {
        m_error = "No hay barras que describir.";
        return;
    }
    m_error = validateDescription(description, project.profileLibrary);
    if (!m_error.empty()) return;
    const auto section = resolveSection(description, project.profileLibrary);
    const auto props = section ? effectiveProperties(*section) : std::nullopt;
    if (!props) {
        m_error = "No se pueden calcular las propiedades de la sección.";
        return;
    }

    description.id = 0;
    description.sectionId = 0;
    description.materialId = 0;
    const BarDescription* reused = nullptr;
    for (const BarDescription& d : descriptions.items)
        if (sameDescription(d, description)) {
            reused = &d;
            break;
        }
    BarDescription chosen;
    if (reused) {
        chosen = *reused;
    } else {
        chosen = description;
        chosen.id = descriptions.next++;
        chosen.sectionId = work.addSection(makeCrossSection(description, *section, *props));
        const Material wanted = makeMaterial(project, description);
        chosen.materialId = findEqualMaterial(work, wanted);
        if (chosen.materialId == 0) chosen.materialId = work.addMaterial(wanted);
        descriptions.items.push_back(chosen);  // ids grow, so the list stays sorted
    }
    m_descriptionId = chosen.id;

    for (int id : targets) {
        Bar* bar = work.findBar(id);
        bar->sectionId = chosen.sectionId;
        bar->materialId = chosen.materialId;
    }
    prune(work, descriptions);
    capture(work, descriptions);
}

// --- RefreshDescriptionsCommand -----------------------------------------------------------------------------------------

RefreshDescriptionsCommand::RefreshDescriptionsCommand(Project& project) : DescriptionCommand(project) {
    StructuralModel work = project.structure;
    const BarDescriptions& descriptions = project.barDescriptions;
    bool changed = false;
    for (const BarDescription& d : descriptions.items) {
        const auto section = resolveSection(d, project.profileLibrary);
        const auto props = section && validateDescription(d, project.profileLibrary).empty() ? effectiveProperties(*section)
                                                                                               : std::nullopt;
        if (!props) {
            m_unresolved.push_back(d.id);
            continue;
        }
        for (CrossSection& cs : work.sections)
            if (cs.id == d.sectionId) {
                CrossSection fresh = makeCrossSection(d, *section, *props);
                fresh.id = cs.id;
                if (!(fresh == cs)) {
                    cs = fresh;
                    changed = true;
                }
            }
    }
    if (!changed) {
        m_error = "Las secciones de las barras ya están actualizadas.";
        return;
    }
    capture(work, descriptions);
}

// --- wrappers -----------------------------------------------------------------------------------------------------------

std::vector<int> barsOfPieces(const Project& project, const std::vector<int>& barIds) {
    std::set<int> out;
    for (int id : barIds) {
        if (!project.structure.findBar(id)) continue;
        if (const Piece* piece = project.structure.pieceOfBar(id)) out.insert(piece->barIds.begin(), piece->barIds.end());
        else out.insert(id);
    }
    return {out.begin(), out.end()};
}

std::string describeBars(Project& project, UndoStack& stack, const std::vector<int>& barIds, const BarDescription& description,
                         bool wholePieces, int* descriptionId) {
    const std::vector<int> targets = wholePieces ? barsOfPieces(project, barIds) : barIds;
    auto command = std::make_unique<DescribeBarsCommand>(project, targets, description);
    if (!command->valid()) return command->error();
    if (descriptionId) *descriptionId = command->descriptionId();
    stack.beginMacro("Describir barras");
    stack.push(std::move(command));
    // After the push the project holds the new sections, so the piece command is built on the final state. NothingToChange
    // (the pieces already carry the type) is not an error.
    const PieceError pieceError = setPieceType(project, stack, targets, description.elementType);
    stack.endMacro();
    (void)pieceError;
    return {};
}

std::string refreshDescriptions(Project& project, UndoStack& stack, std::vector<int>* unresolved) {
    auto command = std::make_unique<RefreshDescriptionsCommand>(project);
    if (unresolved) *unresolved = command->unresolved();
    if (!command->valid()) return {};
    stack.push(std::move(command));
    return {};
}

// --- queries ------------------------------------------------------------------------------------------------------------

const BarDescription* descriptionOfBar(const Project& project, int barId) {
    const Bar* bar = project.structure.findBar(barId);
    return bar ? project.barDescriptions.findBySection(bar->sectionId) : nullptr;
}

bool isBarDescribed(const Project& project, int barId) {
    const Bar* bar = project.structure.findBar(barId);
    return bar && bar->sectionId != 0;
}

std::vector<int> undescribedBars(const Project& project) {
    std::vector<int> out;
    for (const Bar& b : project.structure.bars)
        if (b.sectionId == 0 || b.materialId == 0) out.push_back(b.id);
    return out;
}

std::string barDescriptionText(const Project& project, int barId) {
    const Bar* bar = project.structure.findBar(barId);
    if (!bar || bar->sectionId == 0) return kNotDescribedText;
    if (const BarDescription* d = project.barDescriptions.findBySection(bar->sectionId))
        return descriptionText(*d, project.profiles.hotRolledSteel);
    const CrossSection* cs = project.structure.findSection(bar->sectionId);
    return cs ? cs->name : std::string(kNotDescribedText);
}

namespace {

bool readsLibrary(const BarDescription& d) {
    return d.materialIcon != BarMaterialIcon::Generic && d.materialIcon != BarMaterialIcon::Concrete && !d.seriesRef.empty();
}

std::vector<int> barsMatching(const Project& project, const std::function<bool(const BarDescription&)>& test) {
    std::vector<int> out;
    for (const Bar& b : project.structure.bars) {
        const BarDescription* d = project.barDescriptions.findBySection(b.sectionId);
        if (d && readsLibrary(*d) && test(*d)) out.push_back(b.id);
    }
    return out;
}

}  // namespace

std::vector<int> barsUsingSeries(const Project& project, const std::string& seriesReference) {
    return barsMatching(project, [&](const BarDescription& d) { return d.seriesRef == seriesReference; });
}

std::vector<int> barsUsingProfile(const Project& project, const std::string& seriesReference, const std::string& profileReference) {
    return barsMatching(
        project, [&](const BarDescription& d) { return d.seriesRef == seriesReference && d.profileRef == profileReference; });
}

}  // namespace cc::core

#include "BarDescribeTool.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QKeySequence>
#include <algorithm>

#include "BarDescribeDialog.h"
#include "Viewport3D.h"
#include "core/bar_description_edit.h"
#include "core/profile_library.h"
#include "core/project_settings.h"

namespace cc::app {

using namespace cc::core;

BarDescribeTool::BarDescribeTool(QObject* parent) : QObject(parent) {
    m_actions = new QActionGroup(this);
    m_actions->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto make = [this](const QString& text, const QString& tip, const QString& tooltip, Mode mode) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setEnabled(false);
        action->setStatusTip(tip);
        action->setToolTip(tooltip);
        m_actions->addAction(action);
        connect(action, &QAction::toggled, this, [this, mode](bool on) {
            if (on)
                setMode(mode);
            else if (m_mode == mode)
                setMode(Mode::None);
        });
        return action;
    };
    // #64621 and #64623 (barras.md section 1); the ribbon tooltip of "Seccion" shows its shortcut: "Sección (Ctrl+d)".
    m_section = make(tr("Sección"), tr("Permite seleccionar el tipo de elemento estructural, el material y la geometría para un grupo de barras."),
                     tr("Sección (Ctrl+d)"), Mode::Section);
    m_section->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_material = make(tr("Material"), tr("Permite seleccionar el material para un grupo de barras."), tr("Material"), Mode::Material);
}

QString BarDescribeTool::statusText(Mode mode) {
    switch (mode) {
    case Mode::Section:
        return tr("Barras - Sección |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    case Mode::Material:
        return tr("Barras - Material |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void BarDescribeTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

void BarDescribeTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void BarDescribeTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void BarDescribeTool::deactivate() {
    if (QAction* checked = m_actions->checkedAction()) checked->setChecked(false);
    setMode(Mode::None);
}

void BarDescribeTool::updateEnabled() {
    const bool project = m_project != nullptr;
    for (QAction* action : {m_section, m_material}) {
        action->setEnabled(project);
        if (!project && action->isChecked()) action->setChecked(false);
    }
}

void BarDescribeTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void BarDescribeTool::setMode(Mode mode) {
    if (mode == m_mode) return;
    if (mode != Mode::None) emit aboutToActivate();
    m_mode = mode;
    configureViewports();
    emit modeChanged(mode);
    emit statusChanged(statusText(mode));
}

void BarDescribeTool::configureViewports() {
    pruneViewports();
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setSnapMode(false);
        viewport->setRubberChain({});
        viewport->setMovingNode(std::nullopt);
        viewport->setNodeLabelsVisible(false);
        viewport->setCoordinateTooltipEnabled(true);
        if (m_mode != Mode::None) {
            viewport->setSelectionFilter(SelectionFilter::only(ElementKind::Bar));
            viewport->setSelectionMode(true);
            viewport->setFocus();
        }
    }
}

int BarDescribeTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void BarDescribeTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.empty()) return;  // RMB with nothing selected does nothing
    if (m_mode == Mode::Section)
        describeBars(bars);
    else
        describeMaterial(bars);
    viewport->clearSelection();  // the selection is emptied and the command goes on
    emit statusChanged(statusText(m_mode));
}

bool BarDescribeTool::describeBars(const std::vector<int>& barIds) {
    if (!m_project || !m_undo || barIds.empty()) return false;
    Project& project = *m_project;
    // Entries of "Perfiles utilizados": one per distinct description, in the order of the first bar. Bars not described yet start
    // from the defaults of the dialog (rolled steel IPE 80).
    std::vector<DescribeEntry> entries;
    std::vector<int> sorted = barIds;
    std::sort(sorted.begin(), sorted.end());
    std::vector<bool> undescribed;
    for (int id : sorted) {
        if (!project.structure.findBar(id)) continue;
        const BarDescription* existing = descriptionOfBar(project, id);
        BarDescription d = existing ? *existing : defaultDescription();
        d.id = d.sectionId = d.materialId = 0;
        auto it = std::find_if(entries.begin(), entries.end(), [&](const DescribeEntry& e) { return sameDescription(e.description, d); });
        if (it == entries.end()) {
            entries.push_back({d, {id}});
            undescribed.push_back(existing == nullptr);
        } else {
            it->barIds.push_back(id);
        }
    }
    if (entries.empty()) return false;
    BarDescribeDialog dialog(entries, project.profileLibrary, project.units.dimensions, true, m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;

    // One undo step: library edits, refresh of the descriptions that read an edited profile, then the descriptions.
    m_undo->beginMacro("Describir barras");
    if (dialog.libraryChanged()) {
        m_undo->push(std::make_unique<ReplaceLibraryCommand>(project, dialog.library(), "Biblioteca de perfiles"));
        refreshDescriptions(project, *m_undo);
    }
    std::string error;
    for (std::size_t i = 0; i < dialog.entries().size() && error.empty(); ++i) {
        const DescribeEntry& edited = dialog.entries()[i];
        const DescribeEntry& original = dialog.originalEntries()[i];
        if (!undescribed[i] && edited.description == original.description) continue;  // untouched entry
        error = cc::core::describeBars(project, *m_undo, edited.barIds, edited.description);
    }
    m_undo->endMacro();
    if (!error.empty()) emit messageRequested(QString::fromStdString(error));
    structureChanged();
    return error.empty();
}

bool BarDescribeTool::describeMaterial(const std::vector<int>& barIds) {
    if (!m_project || barIds.empty()) return false;
    const Project& project = *m_project;
    // Groups: "Acero laminado" is always there; the group of the material of the bars only when every bar is of it (the three
    // measured cases: concrete alone, timber alone, mixed selection; describir_barras.md section 9, PROVISIONAL rule).
    bool allConcrete = true, allTimber = true;
    for (int id : barIds) {
        const BarDescription* d = descriptionOfBar(project, id);
        allConcrete = allConcrete && d && d->materialIcon == BarMaterialIcon::Concrete;
        allTimber = allTimber && d && d->materialIcon == BarMaterialIcon::Timber;
    }
    std::vector<DescribeMaterialDialog::GroupSpec> groups;
    QStringList steel;
    for (const std::string& g : hotRolledSteelGrades()) steel << QString::fromStdString(g);
    groups.push_back({DescribeMaterialDialog::Group::RolledSteel, QString::fromStdString(project.profiles.hotRolledSteel), steel});
    if (allConcrete) {
        QStringList grades;
        for (const std::string& g : concreteGrades()) grades << QString::fromStdString(g);
        groups.push_back({DescribeMaterialDialog::Group::Concrete, QString::fromStdString(project.profiles.concrete), grades});
    } else if (allTimber) {
        QStringList grades;  // TODO(captura): the combo of timber was not opened in the original
        for (const std::string& g : timberStrengthClasses()) grades << QString::fromStdString(g);
        groups.push_back({DescribeMaterialDialog::Group::Timber, QString::fromStdString(project.profiles.timber.strengthClass), grades});
    }
    DescribeMaterialDialog dialog(groups, m_dialogParent);
    return run(dialog) == QDialog::Accepted;
}

}  // namespace cc::app

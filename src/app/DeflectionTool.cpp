#include "DeflectionTool.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <cmath>

#include "Icons.h"
#include "PiecePicture.h"
#include "RangedEdit.h"
#include "UserMessages.h"
#include "Viewport3D.h"

namespace cc::app {

using namespace cc::core;

namespace {

void setBold(QWidget* widget, bool bold) {
    QFont font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}

/// Grey header band of the dialogs («Plano xy», measured: gf_10b_c).
QWidget* headerBand(const QString& text, QWidget* parent) {
    auto* band = new QFrame(parent);
    band->setObjectName(QStringLiteral("deflectionHeader"));
    band->setStyleSheet(QStringLiteral("QFrame#deflectionHeader { background: #DDDDDD; }"));
    auto* row = new QHBoxLayout(band);
    row->setContentsMargins(5, 1, 4, 1);
    row->addWidget(new QLabel(text, band));
    return band;
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(QStringLiteral("color: #C9C9C9;"));
    return line;
}

/// Unticking a block HIDES its controls but the dialog keeps its size (measured, gf_30_limit0 / gf_32_limit_rel1).
void setShownKeepingSpace(QWidget* widget, bool shown) {
    QSizePolicy policy = widget->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    widget->setSizePolicy(policy);
    widget->setVisible(shown);
}

double round3(double value) { return std::round(value * 1000.0) / 1000.0; }

}  // namespace

// ---- "Grupo de flecha" --------------------------------------------------------------------------------------------------------

DeflectionTypeDialog::DeflectionTypeDialog(QWidget* parent) : CypeDialog(tr("Grupo de flecha"), parent) {
    setFixedWidth(206);
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    for (int plane = 0; plane < 2; ++plane) {
        const auto i = static_cast<std::size_t>(plane);
        outer->addWidget(headerBand(plane == 0 ? tr("Plano xy") : tr("Plano xz"), body));
        m_group[i] = new QButtonGroup(this);
        m_group[i]->setExclusive(true);
        for (int t = 0; t < 3; ++t) {
            const auto type = static_cast<DeflectionType>(t);
            auto* radio = new QRadioButton(QString::fromStdString(deflectionTypeName(type)), body);
            radio->setContentsMargins(4, 0, 0, 0);
            m_radios[i][static_cast<std::size_t>(t)] = radio;
            m_group[i]->addButton(radio, t);
            outer->addWidget(radio);
            connect(radio, &QRadioButton::toggled, this, [radio](bool on) { setBold(radio, on); });
        }
        m_radios[i][0]->setChecked(true);  // default: Secante (bold)
        if (plane == 0) outer->addSpacing(8);
    }
    outer->addStretch(1);
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

DeflectionType DeflectionTypeDialog::type(int plane) const {
    const int id = m_group[static_cast<std::size_t>(plane)]->checkedId();
    return id < 0 ? DeflectionType::Secant : static_cast<DeflectionType>(id);
}

void DeflectionTypeDialog::setType(int plane, DeflectionType type) { radio(plane, type)->setChecked(true); }

QRadioButton* DeflectionTypeDialog::radio(int plane, DeflectionType type) const {
    return m_radios[static_cast<std::size_t>(plane)][static_cast<std::size_t>(type)];
}

// ---- "Flecha Límite" ----------------------------------------------------------------------------------------------------------

double DeflectionGroupLimitDialog::presetDivisor(int choice) {
    static constexpr double divisors[kPresetCount] = {250.0, 300.0, 400.0, 500.0};
    return divisors[choice];
}

DeflectionGroupLimitDialog::DeflectionGroupLimitDialog(QWidget* parent) : CypeDialog(tr("Flecha Límite"), parent) {
    setFixedWidth(390);
    auto* body = new QWidget(this);
    auto* outer = new QVBoxLayout(body);
    outer->setContentsMargins(8, 0, 8, 0);
    outer->setSpacing(2);
    outer->addWidget(new PiecePicture(body), 0, Qt::AlignHCenter);
    outer->addWidget(separator(body));
    for (int s = 0; s < 4; ++s) buildAbsolute(body, outer, static_cast<LimitSlot>(s));
    for (int s = 4; s < 8; ++s) {
        outer->addWidget(separator(body));
        buildRelative(body, outer, static_cast<LimitSlot>(s));
    }
    outer->addStretch(1);
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->addWidget(body, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

void DeflectionGroupLimitDialog::buildAbsolute(QWidget* parent, QVBoxLayout* layout, LimitSlot slot) {
    const std::size_t i = index(slot);
    auto* row = new QWidget(parent);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 2, 0, 2);
    m_check[i] = new QCheckBox(QString::fromStdString(deflectionLimitName(slot)), row);
    line->addWidget(m_check[i], 1);
    auto* box = new QWidget(row);
    auto* boxLayout = new QHBoxLayout(box);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    m_edit[i] = new RangedEdit(QString::fromStdString(deflectionLimitName(slot)), {kMinAbsoluteDeflectionLimit, 1.0e7}, 1, box);
    m_edit[i]->setFixedWidth(56);
    m_edit[i]->setAlignment(Qt::AlignRight);
    m_edit[i]->setValue(kDefaultAbsoluteDeflectionLimit);
    boxLayout->addWidget(m_edit[i]);
    auto* unit = new QLabel(tr("mm"), box);
    unit->setFixedWidth(26);
    boxLayout->addWidget(unit);
    line->addWidget(box);
    layout->addWidget(row);
    setShownKeepingSpace(box, false);
    connect(m_check[i], &QCheckBox::toggled, box, [box](bool on) { setShownKeepingSpace(box, on); });
}

void DeflectionGroupLimitDialog::buildRelative(QWidget* parent, QVBoxLayout* layout, LimitSlot slot) {
    const std::size_t i = index(slot);
    m_check[i] = new QCheckBox(QString::fromStdString(deflectionLimitName(slot)), parent);
    layout->addWidget(m_check[i]);
    auto* row = new QWidget(parent);
    row->setFixedHeight(30);
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(6);
    m_group[i] = new QButtonGroup(this);
    m_group[i]->setExclusive(true);
    const QString names[5] = {tr("L / 250"), tr("L / 300"), tr("L / 400"), tr("L / 500"), tr("L / ?")};
    for (int k = 0; k < 5; ++k) {
        auto* radio = new QRadioButton(names[k], row);
        m_radios[i][static_cast<std::size_t>(k)] = radio;
        m_group[i]->addButton(radio, k);
        line->addWidget(radio);
        connect(radio, &QRadioButton::toggled, this, [radio](bool on) { setBold(radio, on); });
    }
    m_edit[i] = new RangedEdit(QString::fromStdString(deflectionLimitName(slot)), {kMinDeflectionDivisor, 1.0e6}, 1, row);
    m_edit[i]->setFixedWidth(46);
    m_edit[i]->setAlignment(Qt::AlignRight);
    m_edit[i]->setValue(kDefaultDeflectionDivisor);
    line->addWidget(m_edit[i]);
    line->addStretch(1);
    m_radios[i][0]->setChecked(true);  // default L / 250 (measured)
    layout->addWidget(row);
    setShownKeepingSpace(row, false);
    m_edit[i]->setVisible(false);
    connect(m_check[i], &QCheckBox::toggled, row, [row](bool on) { setShownKeepingSpace(row, on); });
    connect(m_radios[i][4], &QRadioButton::toggled, m_edit[i], [edit = m_edit[i]](bool on) { edit->setVisible(on); });
}

void DeflectionGroupLimitDialog::setLimits(const DeflectionLimits& limits) {
    for (int s = 0; s < kLimitSlotCount; ++s) {
        const auto slot = static_cast<LimitSlot>(s);
        const std::size_t i = index(slot);
        const DeflectionLimitItem& item = limits.item(slot);
        m_check[i]->setChecked(item.enabled);
        if (isAbsoluteLimitSlot(slot)) {
            m_edit[i]->setValue(item.absoluteMm);
            continue;
        }
        int choice = 4;
        for (int k = 0; k < kPresetCount; ++k)
            if (item.relativeDivisor == presetDivisor(k)) choice = k;
        m_radios[i][static_cast<std::size_t>(choice)]->setChecked(true);
        m_edit[i]->setValue(item.relativeDivisor);
    }
}

DeflectionLimits DeflectionGroupLimitDialog::limits() const {
    DeflectionLimits out;
    for (int s = 0; s < kLimitSlotCount; ++s) {
        const auto slot = static_cast<LimitSlot>(s);
        const std::size_t i = index(slot);
        DeflectionLimitItem& item = out.item(slot);
        item.enabled = m_check[i]->isChecked();
        if (isAbsoluteLimitSlot(slot)) {
            item.absoluteMm = m_edit[i]->value();
            continue;
        }
        const int choice = m_group[i]->checkedId();
        item.relativeDivisor = choice >= 0 && choice < kPresetCount ? presetDivisor(choice) : m_edit[i]->value();
    }
    return out;
}

bool DeflectionGroupLimitDialog::commitAll() {
    for (RangedEdit* edit : m_edit)
        if (edit && edit->isVisible() && edit->hasFocus() && !edit->commit()) return false;
    return true;
}

void DeflectionGroupLimitDialog::accept() {
    if (!commitAll()) return;
    if (const auto issue = checkDeflectionLimits(limits())) {
        messages::show(this, messages::Kind::Warning, QString::fromStdString(issue->text));
        return;
    }
    CypeDialog::accept();
}

// ---- tool ---------------------------------------------------------------------------------------------------------------------

DeflectionTool::DeflectionTool(QObject* parent) : QObject(parent) {
    // Drop-down of the ribbon button (gf_02_menu): a title row «Grupos de flecha» over Crear, Editar and Flecha Límite.
    m_menu = new QMenu(tr("Grupos de flecha"));
    connect(this, &QObject::destroyed, m_menu, &QObject::deleteLater);
    auto* title = new QWidgetAction(m_menu);
    auto* titleLabel = new QLabel(tr("Grupos de flecha"));
    titleLabel->setStyleSheet(QStringLiteral("QLabel { background: white; border: 1px solid #A0A0A0; padding: 4px 8px; margin: 2px; }"));
    title->setDefaultWidget(titleLabel);
    m_menu->addAction(title);
    const auto make = [this](const QString& text, const QString& icon, const QString& tip, Mode mode) {
        QAction* action = m_menu->addAction(themedIcon(icon), text);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, [this, mode] { start(mode); });
        return action;
    };
    // Tooltips: the descriptions of the original (#64633-#64637; the assignment to each entry is inferred, grupos_flecha.md section 1).
    m_create = make(tr("Crear"), QStringLiteral("deflection_create"),
                    tr("Permite limitar la flecha en una barra dividida por varios nudos intermedios, de forma que tenga un tratamiento en conjunto, como si no estuviera dividida. "
                       "Esto es posible si los nudos están alineados."),
                    Mode::Create);
    m_edit = make(tr("Editar"), QStringLiteral("deflection_edit"),
                  tr("Permite editar un grupo de flecha existente para cambiar el tipo de cálculo (tangente o secante)."), Mode::Edit);
    m_limit = make(tr("Flecha Límite"), QStringLiteral("deflection_limit"),
                   tr("Permite configurar los límites de flecha en las barras según sus ejes locales."), Mode::Limit);
    m_menu->setIcon(themedIcon(QStringLiteral("bar_deflection")));
    m_menu->setToolTip(tr("Permite configurar los límites de flecha en las barras según sus ejes locales."));
    updateEnabled();
}

QString DeflectionTool::statusText(Mode mode) {
    const QString group = tr(" - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.");
    switch (mode) {
        case Mode::Create: return tr("Grupos de flecha - Crear |") + QLatin1Char(' ') + tr(" - LMB: Seleccione un elemento.");
        case Mode::Edit: return tr("Grupos de flecha - Editar |") + QLatin1Char(' ') + group;
        case Mode::Limit: return tr("Grupos de flecha - Flecha Límite |") + QLatin1Char(' ') + group;
        case Mode::None: break;
    }
    return tr("Seleccione una opción del menú.");
}

void DeflectionTool::pruneViewports() {
    m_viewports.removeIf([](const QPointer<Viewport3D>& v) { return v.isNull(); });
}

int DeflectionTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

void DeflectionTool::setProject(Project* project, UndoStack* undo) {
    deactivate();
    m_project = project;
    m_undo = undo;
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->setDeflection(m_project ? &m_project->deflection : nullptr);
    updateEnabled();
}

void DeflectionTool::addViewport(Viewport3D* viewport) {
    if (!viewport || m_viewports.contains(viewport)) return;
    m_viewports.push_back(viewport);
    viewport->setDeflection(m_project ? &m_project->deflection : nullptr);
    connect(viewport, &Viewport3D::actOnSelection, this, [this, viewport] { onActOnSelection(viewport); });
    connect(viewport, &Viewport3D::selectionChanged, this, [this, viewport] { onSelectionChanged(viewport); });
    connect(viewport, &Viewport3D::escapePressed, this, [this] { deactivate(); });
    if (m_mode != Mode::None) configureViewports();
}

void DeflectionTool::updateEnabled() {
    const bool bars = m_project && !m_project->structure.bars.empty();
    for (QAction* action : {m_create, m_edit, m_limit}) action->setEnabled(bars);
    m_menu->setEnabled(bars);
    m_menu->menuAction()->setEnabled(bars);
    if (!bars) deactivate();
}

void DeflectionTool::structureChanged() {
    updateEnabled();
    pruneViewports();
    if (m_pending && !(m_project && m_project->structure.findNode(*m_pending))) m_pending.reset();
    for (const QPointer<Viewport3D>& viewport : m_viewports) viewport->refreshStructure();
}

void DeflectionTool::clearPending() {
    m_pending.reset();
    for (const QPointer<Viewport3D>& viewport : m_viewports)
        if (viewport) viewport->clearSelection();
}

void DeflectionTool::start(Mode mode) {
    if (!m_project || mode == Mode::None) return;
    emit aboutToActivate();
    m_pending.reset();
    m_mode = mode;
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));  // after the viewports: setSelectionMode announces its own, generic hint
}

void DeflectionTool::deactivate() { stop(); }

void DeflectionTool::stop() {
    if (m_mode == Mode::None) return;
    m_mode = Mode::None;
    m_pending.reset();
    configureViewports();
    emit modeChanged(m_mode);
    emit statusChanged(statusText(m_mode));
}

void DeflectionTool::configureViewports() {
    pruneViewports();
    const bool on = m_mode != Mode::None;
    for (const QPointer<Viewport3D>& viewport : m_viewports) {
        viewport->setSelectionMode(false);
        viewport->setBarAxesVisible(on);  // local axes of every bar (gf_20_edit_active)
        viewport->setDeflectionLabelsVisible(on);
        if (!on) continue;
        viewport->setSelectionFilter(SelectionFilter::only(m_mode == Mode::Create ? ElementKind::Node : ElementKind::Bar));
        viewport->setSelectionMode(true);
        viewport->setFocus();
    }
}

bool DeflectionTool::createBetween(int node1, int node2) {
    if (!m_project || !m_undo) return false;
    // The checks come first: the aviso of #64227 / #64228 replaces the dialog (grupos_flecha.md section 4.2 / 4.3).
    const DeflectionError error = checkCreateDeflectionGroup(*m_project, node1, node2);
    if (error != DeflectionError::None) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(deflectionErrorMessage(error)));
        return false;
    }
    DeflectionTypeDialog dialog(m_dialogParent);
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    const DeflectionError result = createDeflectionGroup(*m_project, *m_undo, node1, node2, dialog.type(0), dialog.type(1));
    if (result != DeflectionError::None) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(deflectionErrorMessage(result)));
        return false;
    }
    return m_undo->count() != before;
}

namespace {
/// Group of the first existing bar of `bars`, from the complete list (for the initial state of the dialogs).
std::optional<DeflectionGroupInfo> groupOfFirstBar(const Project& project, const std::vector<int>& bars) {
    const std::vector<DeflectionGroupInfo> list = deflectionGroups(project.deflection, project.structure);
    const int index = deflectionGroupIndexOfBar(list, bars.front());
    if (index < 0) return std::nullopt;
    return list[static_cast<std::size_t>(index)];
}
}  // namespace

bool DeflectionTool::editTypes(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> bars = existingDeflectionBars(*m_project, barIds);
    if (bars.empty()) return false;
    DeflectionTypeDialog dialog(m_dialogParent);
    if (const auto group = groupOfFirstBar(*m_project, bars)) {  // initial state: the types of the first group (TODO(medir): mixed selections)
        dialog.setType(0, group->xy);
        dialog.setType(1, group->xz);
    }
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    editDeflectionTypes(*m_project, *m_undo, bars, dialog.type(0), dialog.type(1));
    return m_undo->count() != before;
}

bool DeflectionTool::editLimits(const std::vector<int>& barIds) {
    if (!m_project || !m_undo) return false;
    const std::vector<int> bars = existingDeflectionBars(*m_project, barIds);
    if (bars.empty()) return false;
    DeflectionGroupLimitDialog dialog(m_dialogParent);
    if (const auto group = groupOfFirstBar(*m_project, bars)) {
        DeflectionLimits shown = group->limits;
        for (DeflectionLimitItem& item : shown.items) {
            item.absoluteMm = round3(item.absoluteMm);
            item.relativeDivisor = round3(item.relativeDivisor);
        }
        dialog.setLimits(shown);
    }
    if (run(dialog) != QDialog::Accepted) return false;
    const std::size_t before = m_undo->count();
    if (const auto issue = setDeflectionLimits(*m_project, *m_undo, bars, dialog.limits())) {
        messages::show(m_dialogParent, messages::Kind::Warning, QString::fromStdString(issue->text));
        return false;
    }
    return m_undo->count() != before;
}

void DeflectionTool::onActOnSelection(Viewport3D* viewport) {
    if (m_mode == Mode::None || m_mode == Mode::Create || !m_project || !m_undo) return;
    const std::vector<int> bars = viewport->selection().ids(ElementKind::Bar);
    if (bars.empty()) return;
    if (m_mode == Mode::Edit) editTypes(bars);
    else editLimits(bars);
    viewport->clearSelection();
    emit statusChanged(statusText(m_mode));
}

void DeflectionTool::onSelectionChanged(Viewport3D* viewport) {
    if (m_mode != Mode::Create || !m_project || !m_undo) return;
    const std::vector<int> nodes = viewport->selection().ids(ElementKind::Node);
    if (nodes.size() != 1) return;
    const int node = nodes.front();
    if (!m_pending) {  // the first node stays orange (selected) while the second one is awaited
        m_pending = node;
        return;
    }
    if (*m_pending == node) return;
    const int first = *m_pending;
    clearPending();
    createBetween(first, node);
    emit statusChanged(statusText(m_mode));
}

}  // namespace cc::app

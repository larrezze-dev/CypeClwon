#include "LimitStatesDialog.h"

#include <QAction>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <optional>

#include "ClickTooltip.h"
#include "Icons.h"
#include "SituationsDialog.h"
#include "UserMessages.h"
#include "UserSituationsDialogs.h"
#include "core/combinations.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }

QLabel* boldLabel(const QString& text, QWidget* parent) {
    return new QLabel(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()), parent);
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

ReportContext contextOf(const StandardsSelection& standards) {
    ReportContext context;
    context.concrete = shortStandardName(standards.concrete);
    context.aluminium = shortStandardName(standards.aluminium);
    context.coldFormed = shortStandardName(standards.coldFormedSteel);
    context.timber = shortStandardName(standards.timber);
    return context;
}

using ReportBuilder = CombinationReport (*)(bool, const ReportContext&);

/// Viewer ("Situaciones") captured in the original for the kind: the concrete / aluminium / ground report exists for Eurocodes, the
/// cold-formed / timber one for the CTE. Others were not captured (the pencil of hot-rolled steel opens no viewer in the original either).
ReportBuilder builderFor(LimitStateKind kind, const StandardsSelection& s) {
    const auto cls = [](const std::string& name) { return normClassOf(name); };
    switch (kind) {
    case LimitStateKind::Concrete:
    case LimitStateKind::Foundation:
    case LimitStateKind::Ground: return cls(s.concrete) == NormClass::Eurocode ? &buildGroundReport : nullptr;
    case LimitStateKind::Aluminium: return cls(s.aluminium) == NormClass::Eurocode ? &buildGroundReport : nullptr;
    case LimitStateKind::ColdFormed: return cls(s.coldFormedSteel) == NormClass::Cte ? &buildColdFormedTimberReport : nullptr;
    case LimitStateKind::Timber: return cls(s.timber) == NormClass::Cte ? &buildColdFormedTimberReport : nullptr;
    case LimitStateKind::Displacements: return [](bool, const ReportContext& c) { return buildDisplacementsReport(false, c); };
    case LimitStateKind::HotRolled: return nullptr;
    }
    return nullptr;
}

QString headingOf(LimitStateKind kind, const StandardsSelection& s) {
    const auto name = [](const std::string& n) { return qs(shortStandardName(n)); };
    switch (kind) {
    case LimitStateKind::Concrete: return QObject::tr("Hormigón: %1").arg(name(s.concrete));
    case LimitStateKind::Foundation:
        return QObject::tr("Hormigón en cimentaciones: %1")
            .arg(normClassOf(s.concrete) == NormClass::StructuralCode ? QObject::tr("Código Estructural / CTE DB-SE C") : name(s.concrete));
    case LimitStateKind::ColdFormed: return QObject::tr("E.L.U. de rotura. Acero conformado: %1").arg(name(s.coldFormedSteel));
    case LimitStateKind::HotRolled: return QObject::tr("E.L.U. de rotura. Acero laminado: %1").arg(name(s.hotRolledSteel));
    case LimitStateKind::Timber: return QObject::tr("E.L.U. de rotura. Madera: %1").arg(name(s.timber));
    case LimitStateKind::Aluminium: return QObject::tr("E.L.U. de rotura. Aluminio: %1").arg(name(s.aluminium));
    case LimitStateKind::Ground: return QObject::tr("Tensiones sobre el terreno");
    case LimitStateKind::Displacements: return QObject::tr("Desplazamientos");
    }
    return {};
}

QToolButton* pencilButton(QWidget* parent, bool enabled, const QString& tip) {
    auto* pencil = new QToolButton(parent);
    pencil->setIcon(themedIcon(QStringLiteral("edit_pencil")));
    pencil->setAutoRaise(true);
    pencil->setEnabled(enabled);
    pencil->setToolTip(tip);
    return pencil;
}

QString pencilTip(bool available) {
    return available ? QObject::tr("Ver las situaciones y combinaciones de acciones")
                     : QObject::tr("Visor no capturado en el original para esta norma (ticket TK-017).");
}

bool isAbove(const QString& text) { return snowZoneFromText(text.toStdString()) == SnowZone::Above1000; }

}  // namespace

// ------------------------------------------------------------------------------ Estados límite

namespace {

/// Deletes the widgets and layouts of a layout (widgets later, hidden at once).
void clearLayout(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        if (QLayout* child = item->layout()) clearLayout(child);
        delete item;
    }
}

}  // namespace

LimitStatesDialog::LimitStatesDialog(const StandardsSelection& standards, const LimitStatesSettings& initial, QWidget* parent)
    : CypeDialog(tr("Estados límite"), parent), m_standards(standards), m_base(initial), m_layout(buildLimitStateLayout(standards)) {
    contentLayout()->setSpacing(3);
    buildBody();
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

void LimitStatesDialog::buildBody() {
    QVBoxLayout* body = contentLayout();
    const LimitStatesSettings initial = m_base;
    const StandardsSelection& standards = m_standards;
    m_blocks.clear();
    m_combos.clear();
    m_radios.clear();
    const ReportContext context = contextOf(standards);
    int headings = 1;  // "Desplazamientos"
    int rows = 1;      // the row of "Desplazamientos"
    int radioRows = 0;
    int separators = 0;
    const CombinationCode effectiveCode = initial.effectiveCode(standards);

    // With a set of the user the original shows, under the heading, "E.L.U. de rotura: <ref>" (and "E.L.S. de fisuración: <ref>" for the
    // kinds that have it) with the pencil of the viewer on the right, instead of the snow combo [V, research 14.3]. Returns the rows added.
    const auto userLines = [&](LimitStateKind kind) {
        int added = 0;
        const auto line = [&](const QString& label, const UserSituationSet* set) {
            if (!set) return;
            auto* row = new QHBoxLayout;
            row->addSpacing(10);
            row->addWidget(new QLabel(QStringLiteral("%1 %2").arg(label, qs(set->reference)), this));
            row->addStretch(1);
            QToolButton* pencil = pencilButton(this, true, pencilTip(true));
            const UserSituationSet copy = *set;
            connect(pencil, &QToolButton::clicked, this, [this, copy] { SituationsDialog(buildUserSetReport(copy), this).exec(); });
            row->addWidget(pencil);
            body->addLayout(row);
            ++added;
        };
        line(tr("E.L.U. de rotura:"), initial.findSet(initial.userSetFor(kind)));
        if (LimitStatesSettings::hasServiceRow(kind))
            line(tr("E.L.S. de fisuración:"), initial.findSet(initial.userServiceSetFor(kind), true));
        return added;
    };
    const auto usesUserSet = [&](LimitStateKind kind) { return initial.findSet(initial.userSetFor(kind)) != nullptr; };

    for (std::size_t b = 0; b < m_layout.blocks.size(); ++b) {
        const LimitStateBlock& block = m_layout.blocks[b];
        if (b > 0) {
            body->addWidget(separator(this));
            ++separators;
        }
        BlockUi ui;
        ui.cls = block.cls;
        bool concreteInBlock = false;
        bool allUser = true;  // every kind of the block that follows the snow combo uses a set of the user
        for (LimitStateKind kind : block.kinds) {
            body->addWidget(boldLabel(headingOf(kind, standards), this));
            ++headings;
            concreteInBlock = concreteInBlock || kind == LimitStateKind::Concrete;
            rows += userLines(kind);
            allUser = allUser && usesUserSet(kind);
        }
        if (block.hasGround) {
            body->addWidget(boldLabel(headingOf(LimitStateKind::Ground, standards), this));
            ++headings;
            rows += userLines(LimitStateKind::Ground);
            allUser = allUser && usesUserSet(LimitStateKind::Ground);
        }
        if (block.hasRadios) {
            auto* codes = new QHBoxLayout;
            codes->addSpacing(10);
            const struct {
                CombinationCode code;
                QString text;
                bool shown;
            } options[] = {{CombinationCode::CTE, tr("CTE"), true},
                           {CombinationCode::EC, tr("EC"), true},
                           {CombinationCode::Mixed, tr("Hormigón: EC, Acero: CTE"), concreteInBlock}};
            for (const auto& o : options) {
                if (!o.shown) continue;
                auto* radio = new QRadioButton(o.text, this);
                radio->setChecked(o.code == effectiveCode);
                radio->setProperty("code", static_cast<int>(o.code));
                codes->addWidget(radio);
                ui.radios.push_back(radio);
                m_radios.push_back(radio);
            }
            codes->addStretch(1);
            body->addLayout(codes);
            ++radioRows;
        }
        if (block.hasSnow && !allUser) {
            const bool ec3 = block.cls == NormClass::Eurocode && m_layout.ec3Snow;
            const std::string& stored = block.cls == NormClass::Eurocode ? initial.groundSnow
                                        : block.cls == NormClass::Cte    ? initial.coldFormedSnow
                                                                         : initial.hotRolledSnow;
            auto* row = new QHBoxLayout;
            row->addSpacing(10);
            ui.label = new QLabel(this);
            row->addWidget(ui.label);
            ui.combo = new QComboBox(this);
            for (const std::string& item : ec3 ? LimitStatesSettings::snowChoicesEc() : LimitStatesSettings::snowChoices())
                ui.combo->addItem(qs(item));
            ui.combo->setCurrentText(qs(LimitStatesSettings::normalisedSnow(stored, ec3)));
            ui.combo->setFixedWidth(ec3 ? 313 : 205);
            row->addWidget(ui.combo);
            row->addStretch(1);
            // Heading of the combo: "Nieve" for Eurocodes and for the EC radio, "Cota de nieve" otherwise (measured).
            const auto retitle = [this, block, label = ui.label, radios = ui.radios] {
                bool ecRadio = block.cls == NormClass::Eurocode;
                for (QRadioButton* r : radios)
                    if (r->isChecked()) ecRadio = r->property("code").toInt() == static_cast<int>(CombinationCode::EC);
                label->setText(ecRadio ? tr("Nieve") : tr("Cota de nieve"));
            };
            retitle();
            for (QRadioButton* r : ui.radios) connect(r, &QRadioButton::toggled, this, retitle);
            // Pencil: the viewer of the block (first kind with a captured report).
            ReportBuilder builder = nullptr;
            for (LimitStateKind kind : block.kinds)
                if (!builder) builder = builderFor(kind, standards);
            if (!builder && block.hasGround) builder = builderFor(LimitStateKind::Ground, standards);
            QComboBox* combo = ui.combo;
            QToolButton* pencil = pencilButton(this, builder != nullptr, pencilTip(builder != nullptr));
            if (builder) connect(pencil, &QToolButton::clicked, this, [this, combo, builder, context] {
                SituationsDialog(builder(isAbove(combo->currentText()), context), this).exec();
            });
            row->addWidget(pencil);
            body->addLayout(row);
            ++rows;
            m_combos.push_back(ui.combo);
        }
        m_blocks.push_back(ui);
    }

    if (m_layout.groundStandalone) {  // concrete is not a Eurocode: the ground stresses use the characteristic actions
        body->addWidget(separator(this));
        ++separators;
        body->addWidget(boldLabel(headingOf(LimitStateKind::Ground, standards), this));
        ++headings;
        if (const int lines = userLines(LimitStateKind::Ground); lines > 0) {
            rows += lines;
        } else {
            auto* row = new QHBoxLayout;
            row->addSpacing(10);
            row->addWidget(new QLabel(tr("Acciones características"), this));
            row->addStretch(1);
            row->addWidget(pencilButton(this, false, pencilTip(false)));
            body->addLayout(row);
            ++rows;
        }
    }

    body->addWidget(separator(this));
    ++separators;
    body->addWidget(boldLabel(tr("Desplazamientos"), this));
    if (const int lines = userLines(LimitStateKind::Displacements); lines == 0) {
        auto* displacement = new QHBoxLayout;
        displacement->addSpacing(10);
        displacement->addWidget(new QLabel(tr("Acciones características"), this));
        displacement->addStretch(1);
        QToolButton* pencil = pencilButton(this, true, pencilTip(true));
        connect(pencil, &QToolButton::clicked, this,
                [this, context] { SituationsDialog(buildDisplacementsReport(false, context), this).exec(); });
        displacement->addWidget(pencil);
        body->addLayout(displacement);
    } else {
        rows += lines - 1;
    }

    auto* configure = new QPushButton(tr("Configurar combinaciones para cada estado límite"), this);
    body->addWidget(configure);
    connect(configure, &QPushButton::clicked, this, [this] {
        const LimitStatesSettings current = value();  // keeps the snow chosen here
        LimitStatesVerifyDialog verify(current, m_standards, this);
        if (verify.exec() != QDialog::Accepted) return;
        m_base = current;
        m_base.userSets = verify.settings().userSets;
        m_base.userSetOfKind = verify.settings().userSetOfKind;
        m_base.userServiceSetOfKind = verify.settings().userServiceSetOfKind;
        clearLayout(contentLayout());  // the lines of the sets in use replace the snow combos: build the window again
        buildBody();
    });
    body->addStretch(1);

    // Size: 358x438 measured for the default standards; the other variants are scaled with the rows they add or lose (heading 18,
    // row 28, radios 24, separator 6: fitted to the default and all-Eurocode captures); the Eurocode snow combo is wider (426).
    const int height = 438 + 18 * (headings - 8) + 28 * (rows - 4) + 24 * (radioRows - 1) + 6 * (separators - 3);
    // "Hormigón en cimentaciones: Código Estructural / CTE DB-SE C" needs 16 px more than the 358 of the default dialog in our font.
    const int width = m_layout.ec3Snow ? 426 : normClassOf(standards.concrete) == NormClass::StructuralCode ? 374 : 358;
    setFixedSize(width, std::max(height, 250));
}

LimitStatesSettings LimitStatesDialog::value() const {
    LimitStatesSettings out = m_base;
    for (const BlockUi& ui : m_blocks) {
        if (ui.combo) {
            const std::string text = ui.combo->currentText().toStdString();
            if (ui.cls == NormClass::Eurocode) out.groundSnow = text;
            else if (ui.cls == NormClass::Cte) out.coldFormedSnow = out.timberSnow = text;
            else out.hotRolledSnow = text;
        }
        for (const QRadioButton* r : ui.radios) {
            if (!r->isChecked()) continue;
            const auto code = static_cast<CombinationCode>(r->property("code").toInt());
            if (code != m_base.effectiveCode(m_standards) || m_base.codeChosen) {
                out.hotRolledCode = code;
                out.codeChosen = true;
            }
        }
    }
    return out;
}

// ------------------------------------------------------------------------------ Estados límite a verificar

LimitStatesVerifyDialog::LimitStatesVerifyDialog(const LimitStatesSettings& settings, const StandardsSelection& standards, QWidget* parent)
    : CypeDialog(tr("Estados límite a verificar"), parent), m_settings(settings), m_standards(standards) {
    setFixedSize(398, 356);
    setHelpable();
    auto* grid = new QGridLayout;
    grid->setColumnMinimumWidth(0, 158);
    grid->setHorizontalSpacing(2);
    grid->setColumnStretch(1, 1);
    for (int i = 0; i < kLimitStateKindCount; ++i) {
        const auto kind = static_cast<LimitStateKind>(i);
        auto* button = new QPushButton(tr(limitStateCaption(kind)), this);
        connect(button, &QPushButton::clicked, this, [this, kind] { editKind(kind); });
        grid->addWidget(button, i, 0);
        auto* value = new QLabel(this);
        grid->addWidget(value, i, 1);
        m_buttons.push_back(button);
        m_values.push_back(value);
    }
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    addHelpItem([this] { return m_buttons.front()->geometry().united(m_buttons.back()->geometry()); },
                tr("Permite establecer unos coeficientes de combinación particulares para cada estado límite."));
    refreshValues();
}

void LimitStatesVerifyDialog::refreshValues() {
    const auto rows = m_settings.rows(m_standards);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        QString text = qs(rows[i].value);
        text.replace(qs(LimitStatesSettings::defaultSnow()), tr("Altitud inferior o igual a 1000 m"));
        text.replace(QStringLiteral("Acciones características"), tr("Acciones características"));
        m_values[i]->setText(text);
    }
}

bool LimitStatesVerifyDialog::editKind(LimitStateKind kind) {
    LimitStateKindDialog dialog(kind, m_standards, m_settings, this);
    if (dialog.exec() != QDialog::Accepted) return false;
    m_settings = dialog.settings();
    refreshValues();
    return true;
}

// ------------------------------------------------------------------------------ Un estado límite

LimitStateKindDialog::LimitStateKindDialog(LimitStateKind kind, const StandardsSelection& standards, const LimitStatesSettings& settings,
                                           QWidget* parent)
    : CypeDialog(tr(limitStateCaption(kind)), parent), m_kind(kind), m_standards(standards), m_settings(settings) {
    const bool twoRows = LimitStatesSettings::hasServiceRow(kind);
    setFixedSize(twoRows ? 450 : 383, twoRows ? 262 : 230);  // 383x230 measured with one row; two rows [N: grown by one row]
    auto* body = contentLayout();
    body->setSpacing(4);

    m_general = new QRadioButton(tr("Datos generales"), this);
    m_general->setStyleSheet(QStringLiteral("font-weight: bold;"));
    body->addWidget(m_general);
    const LimitStatesSettings::Effective effective = settings.effective(kind, standards);
    auto* generalRow = new QHBoxLayout;
    generalRow->addSpacing(18);
    // "Cota de nieve: ..." with the CTE (and the structural code), "Nieve: ..." with the Eurocodes [V, research 14.3].
    QString text = effective.characteristic ? tr("Acciones características")
                   : effective.code == "CTE" ? tr("Cota de nieve: %1").arg(qs(effective.snow))
                                             : tr("Nieve: %1").arg(qs(effective.snow));
    if (effective.code.empty() && !effective.characteristic) text = QString();
    auto* info = new QLabel(text, this);
    info->setStyleSheet(QStringLiteral("color: #606060;"));
    generalRow->addWidget(info, 1);
    ReportBuilder builder = builderFor(kind, standards);
    auto* pencil = pencilButton(this, builder != nullptr, pencilTip(builder != nullptr));
    if (builder) {
        const ReportContext context = contextOf(standards);
        const bool high = isAbove(qs(effective.snow));
        connect(pencil, &QToolButton::clicked, this, [this, builder, context, high] { SituationsDialog(builder(high, context), this).exec(); });
    }
    generalRow->addWidget(pencil);
    body->addLayout(generalRow);
    m_generalInfo = info;
    m_generalPencil = pencil;
    m_hasGeneralReport = builder != nullptr;

    m_user = new QRadioButton(tr("Situaciones de proyecto introducidas por el usuario"), this);
    m_user->setStyleSheet(QStringLiteral("font-weight: bold;"));
    body->addWidget(m_user);
    body->addLayout(buildRow(false));
    if (twoRows) body->addLayout(buildRow(true));
    body->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    refreshCombo(false, qs(settings.userSetFor(kind)));
    if (m_serviceSets) refreshCombo(true, qs(settings.userServiceSetFor(kind)));
    const bool anyUser = !settings.userSetFor(kind).empty() || (twoRows && !settings.userServiceSetFor(kind).empty());
    (anyUser ? m_user : m_general)->setChecked(true);
    connect(m_general, &QRadioButton::toggled, this, [this] { updateEnabled(); });
    updateEnabled();
}

QStringList LimitStateKindDialog::libraryMenuEntries() {
    return {QObject::tr("Editar biblioteca"), QObject::tr("Importar de biblioteca"), QObject::tr("Exportar a biblioteca")};  // #94337, #94335, #94336
}

/// One row of the user sets: [label] combo, bar (new, delete, copy, edit, library menu), arrow (E.L.U. row only: "Crear a partir de las
/// situaciones de la norma") and the pencil with the listing. The two-row kinds label them "E.L.U. de rotura" / "E.L.S. de fisuración".
QLayout* LimitStateKindDialog::buildRow(bool service) {
    auto* row = new QHBoxLayout;
    row->setSpacing(2);
    QComboBox* combo = new QComboBox(this);
    combo->setFixedWidth(100);
    if (LimitStatesSettings::hasServiceRow(m_kind)) {
        auto* label = new QLabel(service ? tr("E.L.S. de fisuración") : tr("E.L.U. de rotura"), this);
        label->setFixedWidth(118);
        row->addWidget(label);
        m_userControls.push_back(label);
    }
    row->addWidget(combo);
    (service ? m_serviceSets : m_sets) = combo;
    const auto tool = [this, row](const QString& icon, const QString& tip, std::function<void()> action) {
        auto* b = new QToolButton(this);
        b->setIcon(themedIcon(icon));
        b->setIconSize(QSize(16, 16));
        b->setAutoRaise(true);
        ClickTooltip::attach(b, tip);  // the tooltips appear after a click, not while resting [V, research 14.3]
        connect(b, &QToolButton::clicked, this, std::move(action));
        row->addWidget(b);
        m_userControls.push_back(b);
        return b;
    };
    tool(QStringLiteral("hyp_plus"), tr("Nuevo"), [this, service] { newSet(service); });
    tool(QStringLiteral("hyp_cross"), tr("Borrar"), [this, service] { deleteSet(service); });
    tool(QStringLiteral("copy"), tr("Copiar"), [this, service] { copySet(service); });
    tool(QStringLiteral("folder_open"), tr("Editar"), [this, service] { editSet(service); });
    auto* book = tool(QStringLiteral("book_arrow"), tr("Gestión de biblioteca"), [] {});
    auto* menu = new QMenu(book);
    for (const QString& entry : libraryMenuEntries())
        connect(menu->addAction(entry), &QAction::triggered, this, [this] {
            messages::show(this, messages::Kind::Information,
                           tr("La biblioteca personal de situaciones de proyecto todavía no está disponible (ticket TK-102)."));
        });
    book->setMenu(menu);
    book->setPopupMode(QToolButton::MenuButtonPopup);
    row->addSpacing(6);
    if (!service) {
        tool(QStringLiteral("arrow_left_blue"), tr("Crear a partir de las situaciones de la norma"), [this] { fromNorm(); });
    }
    row->addStretch(1);
    // Pencil of this row (measured): tooltip #156478, with no set it says "Debe introducir, al menos, una situación de proyecto.".
    auto* userPencil = pencilButton(this, true, tr("Muestra un listado con las situaciones de proyecto, con y sin acciones sísmicas, en el que se muestran los coeficientes parciales de seguridad (mayoración de acciones) y los coeficientes de combinación para cada tipo de acción (naturaleza)."));
    ClickTooltip::attach(userPencil, userPencil->toolTip());
    connect(userPencil, &QToolButton::clicked, this, [this, service] { viewSet(service); });
    row->addWidget(userPencil);
    m_userControls.push_back(userPencil);
    m_userControls.push_back(combo);
    return row;
}

void LimitStateKindDialog::refreshCombo(bool service, const QString& select) {
    QComboBox* combo = service ? m_serviceSets : m_sets;
    if (!combo) return;
    combo->clear();
    for (std::size_t i = 0; i < m_settings.userSets.size(); ++i)  // each library lists its own sets; the item data is the index
        if (m_settings.userSets[i].service == service) combo->addItem(qs(m_settings.userSets[i].reference), static_cast<int>(i));
    combo->setCurrentIndex(combo->findText(select));  // -1 = empty (measured: the combo starts empty)
}

void LimitStateKindDialog::updateEnabled() {
    for (QWidget* w : m_userControls) w->setEnabled(m_user->isChecked());
    m_generalInfo->setEnabled(m_general->isChecked());
    m_generalPencil->setEnabled(m_general->isChecked() && m_hasGeneralReport);
}

std::vector<std::string> LimitStateKindDialog::existingCategories() const {
    for (const UserSituationSet& s : m_settings.userSets)
        if (!s.categories.empty()) return s.categories;
    return {};
}

void LimitStateKindDialog::newSet(bool service) {
    UserSituationSet fresh;
    fresh.service = service;
    SituationSetDialog dialog(fresh, {}, true, this);  // the repeated reference is only detected when accepting this window (#94676)
    dialog.setProjectHasSeismic(m_projectHasSeismic);
    dialog.setExistingCategories(existingCategories());
    if (dialog.exec() != QDialog::Accepted) return;
    m_settings.userSets.push_back(dialog.set());
    refreshCombo(service, qs(dialog.set().reference));
}

void LimitStateKindDialog::editSet(bool service) {
    QComboBox* combo = setCombo(service);
    if (!combo || combo->currentIndex() < 0) return;  // measured: Editar with an empty combo does nothing
    const auto index = static_cast<std::size_t>(combo->currentData().toInt());
    const std::string old = m_settings.userSets[index].reference;
    SituationSetDialog dialog(m_settings.userSets[index], {}, false, this);
    dialog.setProjectHasSeismic(m_projectHasSeismic);
    if (dialog.exec() != QDialog::Accepted) return;
    const UserSituationSet edited = dialog.set();
    for (std::string& ref : service ? m_settings.userServiceSetOfKind : m_settings.userSetOfKind)
        if (ref == old) ref = edited.reference;  // the limit states keep using the renamed set
    m_settings.userSets[index] = edited;
    refreshCombo(service, qs(edited.reference));
}

void LimitStateKindDialog::copySet(bool service) {
    QComboBox* combo = setCombo(service);
    if (!combo || combo->currentIndex() < 0) return;
    UserSituationSet copy = m_settings.userSets[static_cast<std::size_t>(combo->currentData().toInt())];
    std::string base = tr("Copia de %1").arg(qs(copy.reference)).toStdString();  // "Copia de %s" (#74963)
    std::string name = base;
    for (int n = 2; m_settings.findSet(name, service); ++n) name = base + " (" + std::to_string(n) + ")";
    copy.reference = name;
    m_settings.userSets.push_back(copy);
    refreshCombo(service, qs(name));
}

void LimitStateKindDialog::deleteSet(bool service) {
    QComboBox* combo = setCombo(service);
    if (!combo || combo->currentIndex() < 0) return;  // measured: Borrar with an empty combo does nothing
    const int index = combo->currentData().toInt();
    const std::string reference = m_settings.userSets[static_cast<std::size_t>(index)].reference;
    // Used by another limit state (or this one before accepting): the original refuses (#156484).
    LimitStatesSettings others = m_settings;
    if (service) others.setUserServiceSetFor(m_kind, std::string{});
    else others.setUserSetFor(m_kind, std::string{});
    if (others.setInUse(reference, service)) {
        messages::show(this, messages::Kind::Warning,
                       tr("No es posible borrar esta situación de proyecto, ya que está siendo usada para la comprobación de otros estados límite."));
        return;
    }
    if (!messages::show(this, messages::Kind::Question, tr("¿Desea borrar la situación de proyecto seleccionada?"))) return;
    m_settings.userSets.erase(m_settings.userSets.begin() + index);
    if (service) m_settings.setUserServiceSetFor(m_kind, std::string{});
    else m_settings.setUserSetFor(m_kind, std::string{});
    refreshCombo(service, QString());
    refreshCombo(!service, QString());  // the indices of the other library moved
}

void LimitStateKindDialog::viewSet(bool service) {
    QComboBox* combo = setCombo(service);
    if (!combo || combo->currentIndex() < 0) {
        messages::show(this, messages::Kind::Warning, tr("Debe introducir, al menos, una situación de proyecto."));
        return;
    }
    SituationsDialog(buildUserSetReport(m_settings.userSets[static_cast<std::size_t>(combo->currentData().toInt())]), this).exec();
}

void LimitStateKindDialog::fromNorm() {
    std::string reference = tr("Norma").toStdString();
    for (int n = 2; m_settings.findSet(reference); ++n) reference = tr("Norma").toStdString() + " " + std::to_string(n);
    const LimitStatesSettings::Effective e = m_settings.effective(m_kind, m_standards);
    m_settings.userSets.push_back(situationsFromNorm(e.code != "CTE", isAbove(qs(e.snow)), reference));
    refreshCombo(false, qs(reference));
}

void LimitStateKindDialog::accept() {
    if (m_user->isChecked()) {
        // Empty combo with the user radio: Error (red icon) "Debe introducir, al menos, una situación de proyecto." (#156481) [V].
        for (const bool service : {false, true}) {
            QComboBox* combo = setCombo(service);
            if (combo && combo->currentIndex() < 0) {
                messages::show(this, messages::Kind::Error, tr(situationErrorMessage(SituationError::NoSetSelected)));
                return;
            }
        }
        // The same reference twice in a library is only detected here: Error #94676 [V, research 14.4].
        for (const bool service : {false, true}) {
            const std::string repeated = m_settings.repeatedReference(service);
            if (!repeated.empty()) {
                messages::show(this, messages::Kind::Error, qs(repeatedReferenceMessage(repeated)));
                return;
            }
        }
        m_settings.setUserSetFor(m_kind, m_sets->currentText().toStdString());
        if (m_serviceSets) m_settings.setUserServiceSetFor(m_kind, m_serviceSets->currentText().toStdString());
    } else {
        m_settings.setUserSetFor(m_kind, std::string{});
        if (m_serviceSets) m_settings.setUserServiceSetFor(m_kind, std::string{});
    }
    CypeDialog::accept();
}

}  // namespace cc::app

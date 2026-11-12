#include "ResultsPanel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QVBoxLayout>

namespace cc::app {

using cc::fem::ResultFamily;

namespace {

QLabel* band(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("QLabel { background: #E6E6E6; padding: 3px 6px; }"));
    return label;
}

QRadioButton* radio(const QString& text, QWidget* parent, bool checked = false) {
    auto* r = new QRadioButton(text, parent);
    r->setChecked(checked);
    r->setStyleSheet(QStringLiteral("QRadioButton:checked { font-weight: bold; }"));
    return r;
}

}  // namespace

QString ResultsPanel::title(Kind kind) {
    switch (kind) {
        case Kind::Displacements: return tr("Propiedades - Desplazamientos");
        case Kind::Reactions: return tr("Propiedades - Reacciones");
        case Kind::Forces: return tr("Propiedades - Esfuerzos");
    }
    return {};
}

unsigned ResultsPanel::rowColour(int row) {
    static const unsigned colours[kForceRows] = {0x008000, 0xFF00FF, 0x000080, 0x800080, 0x008080, 0x0000FF, 0xFF3F00, 0x00BFFF, 0x667FCC, 0xCC7F66};
    return row >= 0 && row < kForceRows ? colours[row] : 0x000000;
}

ResultsPanel::ResultsPanel(Kind kind, QWidget* parent) : CypeDialog(title(kind), parent), m_kind(kind) {
    setModal(false);
    setMaximizable();
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);
    auto* body = new QWidget(this);
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(4, 4, 4, 4);
    column->setSpacing(4);
    const auto emitChanged = [this] {
        if (!m_loading) emit changed();
    };

    if (kind == Kind::Forces) {
        // rp_07_eff_panel.png: Leyes / Envolventes, Todas las barras / Sólo las barras seleccionadas (+ Consultar valores), the ten rows with
        // their scales, five check boxes and «Comb. seleccionada».
        m_laws = radio(tr("Leyes"), body, true);
        auto* envelopes = radio(tr("Envolventes"), body);
        envelopes->setEnabled(false);  // envelopes: not wired yet
        auto* g1 = new QButtonGroup(this);
        g1->addButton(m_laws);
        g1->addButton(envelopes);
        column->addWidget(m_laws);
        column->addWidget(envelopes);
        column->addSpacing(4);
        m_allBars = radio(tr("Todas las barras"), body, true);
        m_selectedOnly = radio(tr("Sólo las barras seleccionadas"), body);
        auto* g2 = new QButtonGroup(this);
        g2->addButton(m_allBars);
        g2->addButton(m_selectedOnly);
        m_query = new QCheckBox(tr("Consultar valores"), body);
        m_query->setEnabled(false);
        column->addWidget(m_allBars);
        column->addWidget(m_selectedOnly);
        auto* queryRow = new QHBoxLayout;
        queryRow->setContentsMargins(18, 0, 0, 0);
        queryRow->addWidget(m_query);
        column->addLayout(queryRow);
        connect(m_selectedOnly, &QRadioButton::toggled, this, [this](bool on) {
            m_query->setEnabled(on);
            m_query->setChecked(on);  // measured: it checks itself
            updateVisibility();
            if (!m_loading) emit changed();
        });
        connect(m_query, &QCheckBox::toggled, this, emitChanged);
        column->addSpacing(4);
        const char* const rows[kForceRows] = {QT_TR_NOOP("Axil (N)"),           QT_TR_NOOP("Cortante y (Vy)"),   QT_TR_NOOP("Cortante z (Vz)"),
                                              QT_TR_NOOP("Momento torsor (Mt)"), QT_TR_NOOP("Momento y (My)"),    QT_TR_NOOP("Momento z (Mz)"),
                                              QT_TR_NOOP("Deformada (D)"),      QT_TR_NOOP("Flecha xy (Fxy)"),   QT_TR_NOOP("Flecha xz (Fxz)"),
                                              QT_TR_NOOP("Flecha (F)")};
        for (int r = 0; r < kForceRows; ++r) {
            auto* rowWidget = new QWidget(body);
            auto* line = new QHBoxLayout(rowWidget);
            line->setContentsMargins(0, 0, 0, 0);
            auto* box = new QCheckBox(tr(rows[r]), rowWidget);
            auto* swatch = new QLabel(rowWidget);
            swatch->setFixedSize(22, 10);
            swatch->setStyleSheet(QStringLiteral("background: #%1;").arg(rowColour(r), 6, 16, QLatin1Char('0')));
            swatch->setVisible(false);  // the colour sample shows only while the row is checked [V]
            auto* edit = new QLineEdit(QString::number(r < kDeformedRow ? 0.1 : 100.0, 'f', 3), rowWidget);
            edit->setValidator(new QDoubleValidator(0.0, 1e9, 6, edit));
            edit->setAlignment(Qt::AlignRight);
            edit->setFixedWidth(80);
            line->addWidget(box, 1);
            line->addWidget(swatch);
            line->addWidget(edit);
            if (r > kDeformedRow) {  // deflections: not wired yet
                box->setEnabled(false);
                edit->setEnabled(false);
            }
            connect(box, &QCheckBox::toggled, swatch, &QLabel::setVisible);
            connect(box, &QCheckBox::toggled, this, emitChanged);
            connect(edit, &QLineEdit::editingFinished, this, emitChanged);
            m_rows[static_cast<std::size_t>(r)] = box;
            m_scales[static_cast<std::size_t>(r)] = edit;
            m_rowWidgets[static_cast<std::size_t>(r)] = rowWidget;
            column->addWidget(rowWidget);
        }
        column->addSpacing(4);
        m_axesOnWindow = new QCheckBox(tr("Ejes sobre el plano de la ventana"), body);
        m_drawExtremes = new QCheckBox(tr("Dibujar valores máximos y mínimos"), body);
        m_viewExtremes = new QCheckBox(tr("Ver valores máximos y mínimos"), body);
        m_viewExtremes->setEnabled(false);
        m_fill = new QCheckBox(tr("Dibujar las leyes con relleno"), body);
        m_fill->setChecked(true);
        m_structure = new QCheckBox(tr("Ver la estructura"), body);
        m_structure->setChecked(true);
        m_axesOnWindow->setEnabled(false);  // not wired yet
        connect(m_drawExtremes, &QCheckBox::toggled, m_viewExtremes, &QCheckBox::setEnabled);
        for (QCheckBox* box : {m_axesOnWindow, m_drawExtremes, m_viewExtremes, m_fill, m_structure}) {
            column->addWidget(box);
            connect(box, &QCheckBox::toggled, this, emitChanged);
        }
        column->addSpacing(4);
    } else {
        auto* selected = radio(tr("Combinación seleccionada"), body, true);
        auto* group = new QButtonGroup(this);
        group->addButton(selected);
        column->addWidget(selected);
        // Displacements: «Envolventes» [V s_lin.png]; reactions: the two foundation envelopes [V rp_03_react_open.png].
        const QStringList envelopes = kind == Kind::Displacements
                                          ? QStringList{tr("Envolventes")}
                                          : QStringList{tr("Envolventes de las combinaciones de equilibrio de cimentación"),
                                                        tr("Envolventes de las combinaciones de tensión sobre el terreno")};
        for (const QString& text : envelopes) {
            auto* r = radio(text, body);
            r->setEnabled(false);  // envelopes: not wired yet
            group->addButton(r);
            column->addWidget(r);
        }
        column->addSpacing(6);
    }

    m_comboGroup = new QWidget(body);
    auto* comboColumn = new QVBoxLayout(m_comboGroup);
    comboColumn->setContentsMargins(0, 0, 0, 0);
    comboColumn->setSpacing(4);
    comboColumn->addWidget(band(tr("Comb. seleccionada"), m_comboGroup));
    m_family = new QComboBox(m_comboGroup);
    m_family->setObjectName(QStringLiteral("resultFamily"));
    m_item = new QComboBox(m_comboGroup);
    m_item->setObjectName(QStringLiteral("resultItem"));
    comboColumn->addWidget(m_family);
    comboColumn->addWidget(m_item);
    column->addWidget(m_comboGroup);

    if (kind != Kind::Forces) {
        column->addSpacing(32);
        const char* const displacement[] = {"Dx", "Dy", "Dz", "Gx", "Gy", "Gz"};
        const char* const reaction[] = {"Rx", "Ry", "Rz", "Mx", "My", "Mz"};
        const char* const* names = kind == Kind::Displacements ? displacement : reaction;
        column->addWidget(band(kind == Kind::Displacements ? tr("Desplazamientos") : tr("Reacciones"), body));
        auto* boxes = new QHBoxLayout;
        boxes->setContentsMargins(4, 2, 4, 2);
        boxes->setSpacing(10);
        for (int k = 0; k < 6; ++k) {
            auto* box = new QCheckBox(QString::fromLatin1(names[k]), body);
            box->setChecked(true);  // all checked by default [V]
            m_rows[static_cast<std::size_t>(k)] = box;
            boxes->addWidget(box);
            connect(box, &QCheckBox::toggled, this, emitChanged);
        }
        boxes->addStretch(1);
        column->addLayout(boxes);
    }
    column->addStretch(1);
    contentLayout()->addWidget(body, 1);
    connect(m_family, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_loading) return;
        fillItems();
        emit changed();
    });
    connect(m_item, &QComboBox::currentIndexChanged, this, emitChanged);
    setMinimumSize(277, 370);
    resize(514, 790);  // s_lin.png: the panel covers the left column
}

void ResultsPanel::updateVisibility() {
    // Measured: with «Sólo las barras seleccionadas» the row Deformada (D) and «Ejes sobre el plano de la ventana» / «Ver valores máximos y
    // mínimos» disappear.
    if (m_kind != Kind::Forces) return;
    const bool all = !m_selectedOnly->isChecked();
    m_rowWidgets[kDeformedRow]->setVisible(all);
    m_axesOnWindow->setVisible(all);
    m_viewExtremes->setVisible(all);
}

void ResultsPanel::setAnalysis(const cc::fem::ProjectAnalysis* analysis) {
    m_analysis = analysis;
    m_loading = true;
    const int keepFamily = m_family->currentIndex() < 0 ? -1 : m_family->currentData().toInt();
    const int keepItem = m_item->currentIndex();
    m_family->clear();
    if (analysis)
        for (ResultFamily f : cc::fem::availableFamilies(*analysis))
            m_family->addItem(QString::fromStdString(cc::fem::resultFamilyName(f)), static_cast<int>(f));
    const int f = m_family->findData(keepFamily);
    m_family->setCurrentIndex(f >= 0 ? f : (m_family->count() > 0 ? 0 : -1));
    fillItems();
    if (f >= 0 && keepItem >= 0 && keepItem < m_item->count()) m_item->setCurrentIndex(keepItem);
    m_loading = false;
}

void ResultsPanel::fillItems() {
    const bool loading = m_loading;
    m_loading = true;
    m_item->clear();
    if (m_analysis)
        if (const auto f = family())
            for (const std::string& name : cc::fem::resultItemNames(*m_analysis, *f)) m_item->addItem(QString::fromStdString(name));
    if (m_item->count() > 0) m_item->setCurrentIndex(0);
    m_loading = loading;
}

std::optional<ResultFamily> ResultsPanel::family() const {
    if (m_family->currentIndex() < 0) return std::nullopt;
    return static_cast<ResultFamily>(m_family->currentData().toInt());
}

int ResultsPanel::item() const { return m_item->currentIndex(); }

cc::fem::ComponentMask ResultsPanel::components() const {
    cc::fem::ComponentMask mask{};
    for (std::size_t k = 0; k < 6; ++k) mask[k] = m_rows[k]->isChecked();
    return mask;
}

bool ResultsPanel::deformed() const { return m_kind == Kind::Forces && !selectedOnly() && m_rows[kDeformedRow]->isChecked(); }
bool ResultsPanel::selectedOnly() const { return m_selectedOnly && m_selectedOnly->isChecked(); }
bool ResultsPanel::queryValues() const { return m_query && m_query->isEnabled() && m_query->isChecked(); }
bool ResultsPanel::fillLaws() const { return !m_fill || m_fill->isChecked(); }
bool ResultsPanel::drawExtremes() const { return m_drawExtremes && m_drawExtremes->isChecked(); }
bool ResultsPanel::showStructure() const { return !m_structure || m_structure->isChecked(); }

double ResultsPanel::scale(int row) const {
    if (row < 0 || row >= kForceRows || !m_scales[static_cast<std::size_t>(row)]) return 0.0;
    bool ok = false;
    const double value = m_scales[static_cast<std::size_t>(row)]->text().toDouble(&ok);
    return ok ? value : (row < kDeformedRow ? 0.1 : 100.0);
}

void ResultsPanel::done(int result) {
    CypeDialog::done(result);
    emit closed();
}

}  // namespace cc::app

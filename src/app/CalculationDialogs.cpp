#include "CalculationDialogs.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

#include "Icons.h"
#include "UserMessages.h"
#include "core/load_case.h"
#include "core/nonlinear_combinations_edit.h"
#include "core/project.h"

namespace cc::app {

using cc::core::NonlinearCombination;
using cc::core::NonlinearCombinationKind;

namespace {

QString qs(const std::string& text) { return QString::fromStdString(text); }

/// Grey section band of the original («Análisis a realizar», «Perfiles»).
QLabel* band(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("QLabel { background: #E6E6E6; padding: 3px 6px; }"));
    return label;
}

QToolButton* barButton(const QString& icon, const QString& tip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setIcon(themedIcon(icon));
    button->setIconSize(QSize(16, 16));
    button->setAutoRaise(true);
    button->setToolTip(tip);
    return button;
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet(QStringLiteral("color: #C8C8C8;"));
    return line;
}

}  // namespace

// ---------------------------------------------------------------- Cálculo

CalculationDialog::CalculationDialog(bool nonlinearElements, bool hasNonlinearCombinations, const CalculationChoices& initial, QWidget* parent)
    : CypeDialog(tr("Cálculo"), parent) {
    setHelpable();
    contentLayout()->setContentsMargins(8, 6, 8, 6);
    contentLayout()->setSpacing(4);
    if (nonlinearElements) {
        // rr_e2_dlg.png: «Análisis a realizar» with the two analyses and the update box indented under the non-linear one.
        contentLayout()->addWidget(band(tr("Análisis a realizar"), this));
        m_linear = new QCheckBox(tr("Análisis lineal de tensión / deformación"), this);
        m_linear->setChecked(initial.linear);
        m_nonlinear = new QCheckBox(tr("Análisis no lineal de tensión / deformación"), this);
        m_nonlinear->setChecked(initial.nonlinear);
        contentLayout()->addWidget(m_linear);
        contentLayout()->addWidget(m_nonlinear);
        if (hasNonlinearCombinations) {
            m_update = new QCheckBox(tr("Actualizar las combinaciones no lineales tras el análisis lineal"), this);
            m_update->setChecked(initial.updateNonlinear);
            auto* row = new QHBoxLayout;
            row->setContentsMargins(16, 6, 0, 2);
            row->addWidget(m_update);
            contentLayout()->addLayout(row);
            m_update->setEnabled(m_nonlinear->isChecked());
            connect(m_nonlinear, &QCheckBox::toggled, m_update, &QCheckBox::setEnabled);
            addHelpItem([this] { return m_update->geometry().translated(m_update->parentWidget()->mapTo(this, QPoint())); },
                        tr("El programa generará automáticamente las combinaciones no lineales tras el análisis lineal."));
        }
        // Grey band with its own check box (clear by default); second-order analysis belongs to T-P10.
        m_secondOrder = new QCheckBox(tr("Análisis de segundo orden elástico"), this);
        m_secondOrder->setStyleSheet(QStringLiteral("QCheckBox { background: #E6E6E6; padding: 3px 4px; }"));
        m_secondOrder->setEnabled(false);
        contentLayout()->addWidget(m_secondOrder);
        contentLayout()->addSpacing(70);
        contentLayout()->addWidget(band(tr("Perfiles"), this));
    } else {
        // cp_61_calcdlg.png. When the original shows this band is [N]: it is in the capture without non-linear elements and not in the other one.
        auto* info = new QLabel(tr("No se ha activado la opción de comprobación de resistencia al fuego."), this);
        info->setStyleSheet(QStringLiteral("QLabel { background: #EAF4FB; border: 1px solid #B9D7EE; padding: 3px 6px 3px 22px; }"));
        auto* icon = new QLabel(info);
        icon->setPixmap(themedIcon(QStringLiteral("report_info")).pixmap(14, 14));
        icon->move(5, 4);
        contentLayout()->addWidget(info);
    }
    m_noDesign = new QRadioButton(tr("No dimensionar perfiles"), this);
    m_noDesign->setChecked(true);
    m_noDesign->setStyleSheet(QStringLiteral("QRadioButton:checked { font-weight: bold; }"));
    auto* quick = new QRadioButton(tr("Dimensionamiento rápido de perfiles"), this);
    auto* optimal = new QRadioButton(tr("Dimensionamiento óptimo de perfiles"), this);
    quick->setEnabled(false);  // profile design: T-P9
    optimal->setEnabled(false);
    auto* group = new QButtonGroup(this);
    for (QRadioButton* r : {m_noDesign, quick, optimal}) {
        group->addButton(r);
        contentLayout()->addWidget(r);
    }
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #C8C8C8;"));
    contentLayout()->addWidget(line);
    contentLayout()->addSpacing(54);
    m_checkBars = new QCheckBox(tr("Comprobar las barras"), this);
    m_checkBars->setChecked(initial.checkBars);
    m_finiteNodes = new QCheckBox(tr("Considerar la dimensión finita de los nudos"), this);
    m_finiteNodes->setChecked(initial.finiteNodes);
    m_finiteNodes->setEnabled(false);  // finite node size: TK-106
    contentLayout()->addWidget(m_checkBars);
    contentLayout()->addWidget(m_finiteNodes);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFixedWidth(nonlinearElements ? 404 : 420);
}

CalculationChoices CalculationDialog::choices() const {
    CalculationChoices c;
    c.linear = m_linear ? m_linear->isChecked() : true;
    c.nonlinear = m_nonlinear ? m_nonlinear->isChecked() : false;
    c.updateNonlinear = m_update ? m_update->isChecked() : true;
    c.checkBars = m_checkBars->isChecked();
    c.finiteNodes = m_finiteNodes->isChecked();
    return c;
}

// ---------------------------------------------------------------- Informe final de cálculo

QString CalculationReportDialog::pageHtml(const std::vector<cc::fem::ReportEntry>& entries) {
    QString html = QStringLiteral("<html><body style='background:#FFFFFF; font-family:Arial;'>");
    html += QStringLiteral("<p align='center' style='font-size:15px; font-weight:bold; margin-top:40px;'>") + tr("Informe final de cálculo").toHtmlEscaped() +
            QStringLiteral("</p>");
    html += QStringLiteral("<p style='font-size:11px; font-weight:bold; margin:24px 0 4px 50px;'>") + tr("Cálculo de la estructura").toHtmlEscaped() +
            QStringLiteral("</p><table style='margin-left:60px;' cellspacing='0' cellpadding='1'>");
    for (const cc::fem::ReportEntry& e : entries) {
        const char* icon = e.level == cc::fem::ReportLevel::Error ? "report_error" : e.level == cc::fem::ReportLevel::Warning ? "report_warning" : "report_info";
        html += QStringLiteral("<tr><td width='26' valign='top'><img src=':/icons/%1.svg' width='9' height='9'></td><td style='font-size:11px;'>")
                    .arg(QLatin1String(icon)) +
                qs(e.text).toHtmlEscaped() + QStringLiteral("</td></tr>");
    }
    html += QStringLiteral("</table></body></html>");
    return html;
}

CalculationReportDialog::CalculationReportDialog(const std::vector<cc::fem::ReportEntry>& entries, QWidget* parent)
    : CypeDialog(tr("Informe final de cálculo"), parent) {
    setMaximizable();
    contentLayout()->setContentsMargins(4, 4, 4, 4);
    contentLayout()->setSpacing(4);
    auto* bar = new QHBoxLayout;
    bar->setSpacing(4);
    auto* zoomIn = barButton(QStringLiteral("view_zoom2"), tr("Ampliar"), this);
    auto* zoomOut = barButton(QStringLiteral("view_zoom_window"), tr("Reducir"), this);
    auto* pages = new QLabel(tr("1 de 1"), this);
    pages->setMinimumWidth(70);
    pages->setAlignment(Qt::AlignCenter);
    auto* share = new QPushButton(tr("Compartir"), this);
    auto* exportButton = new QPushButton(tr("Exportar"), this);
    share->setEnabled(false);  // listings phase
    exportButton->setEnabled(false);
    auto* print = new QPushButton(themedIcon(QStringLiteral("print")), tr("Imprimir..."), this);
    for (QPushButton* b : {share, exportButton, print}) b->setFlat(true);
    bar->addWidget(zoomIn);
    bar->addWidget(zoomOut);
    bar->addWidget(separator(this));
    bar->addWidget(pages);
    bar->addWidget(separator(this));
    bar->addWidget(share);
    bar->addWidget(exportButton);
    bar->addWidget(print);
    bar->addStretch(1);
    contentLayout()->addLayout(bar);
    m_page = new QTextBrowser(this);
    m_page->setObjectName(QStringLiteral("reportPage"));
    m_page->setHtml(pageHtml(entries));
    contentLayout()->addWidget(m_page, 1);
    connect(zoomIn, &QToolButton::clicked, m_page, [this] { m_page->zoomIn(1); });
    connect(zoomOut, &QToolButton::clicked, m_page, [this] { m_page->zoomOut(1); });
    connect(print, &QPushButton::clicked, this, [this] {
        QPrinter printer;
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() == QDialog::Accepted) m_page->print(&printer);
    });
    resize(712, 268);  // s_5.png: 712 x 268 px
}

// ---------------------------------------------------------------- Generar combinaciones

NonlinearGenerateDialog::NonlinearGenerateDialog(const cc::core::NonlinearKinds& initial, QWidget* parent)
    : CypeDialog(tr("Generar combinaciones para las distintas situaciones de proyecto"), parent) {
    contentLayout()->setContentsMargins(10, 8, 10, 8);
    contentLayout()->setSpacing(6);
    for (int k = 0; k < cc::core::kNonlinearCombinationKindCount; ++k) {
        const auto kind = static_cast<NonlinearCombinationKind>(k);
        auto* box = new QCheckBox(qs(cc::core::nonlinearCombinationKindName(kind)), this);
        const bool supported = kind == NonlinearCombinationKind::Strength || kind == NonlinearCombinationKind::Displacements;
        box->setChecked(supported && initial[static_cast<std::size_t>(k)]);
        if (!supported) {
            box->setEnabled(false);
            box->setToolTip(tr("Este tipo de combinación aún no está disponible en esta versión."));
        }
        m_boxes[static_cast<std::size_t>(k)] = box;
        contentLayout()->addWidget(box);
    }
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    setFixedWidth(416);  // s_9.png
}

cc::core::NonlinearKinds NonlinearGenerateDialog::kinds() const {
    cc::core::NonlinearKinds out{};
    for (std::size_t k = 0; k < out.size(); ++k) out[k] = m_boxes[k]->isEnabled() && m_boxes[k]->isChecked();
    return out;
}

// ---------------------------------------------------------------- Combinaciones no lineales

namespace {

/// Rows of «Tipos de combinación» [R: texts of combsanl #377831-#377838; the list of s_10.png shows them truncated].
const char* const kKindRows[] = {QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Resistencia"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Resistencia en situación de incendio"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Fisuración"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Fisuración bajo cargas de larga duración"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Desplazamientos"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Equilibrio de los elementos de cimentación"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Tensiones en el terreno (E.L.U.)"),
                                 QT_TRANSLATE_NOOP("cc::app::NonlinearCombinationsDialog", "Tensiones en el terreno (E.L.S.)")};
constexpr int kKindRowCount = 8;

}  // namespace

int NonlinearCombinationsDialog::kindOfRow(int row) {
    static const int map[kKindRowCount] = {0, -1, 1, 2, 3, 4, 5, 5};
    return row >= 0 && row < kKindRowCount ? map[row] : -1;
}

NonlinearCombinationsDialog::NonlinearCombinationsDialog(const cc::core::Project& project, QWidget* parent)
    : CypeDialog(tr("Combinaciones no lineales"), parent), m_project(project), m_set(project.nonlinearCombinations) {
    setMaximizable();
    contentLayout()->setContentsMargins(6, 6, 6, 6);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // Left: toolbar + list.
    auto* left = new QFrame(splitter);
    left->setFrameShape(QFrame::StyledPanel);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(2, 2, 2, 2);
    auto* bar = new QHBoxLayout;
    bar->setSpacing(1);
    m_add = barButton(QStringLiteral("hyp_plus"), tr("Añadir elemento a la lista"), left);
    m_copy = barButton(QStringLiteral("copy"), tr("Copiar elemento seleccionado de la lista"), left);
    m_up = barButton(QStringLiteral("arrow_up"), tr("Mover un lugar hacia arriba el elemento seleccionado de la lista"), left);
    m_down = barButton(QStringLiteral("arrow_down"), tr("Mover un lugar hacia abajo el elemento seleccionado de la lista"), left);
    m_delete = barButton(QStringLiteral("delete_marked"), tr("Borrar / Suprimir elemento seleccionado de la lista"), left);
    m_deleteAll = barButton(QStringLiteral("trash"), tr("Borrar todos los elementos de la lista"), left);
    m_generate = barButton(QStringLiteral("arrow_left_blue"), tr("Generar combinaciones para las distintas situaciones de proyecto"), left);
    m_brush = barButton(QStringLiteral("brush"), QString(), left);
    m_brush->setEnabled(false);  // its function was not identified [N]
    bar->addWidget(m_add);
    bar->addWidget(m_copy);
    bar->addWidget(separator(left));
    bar->addWidget(m_up);
    bar->addWidget(m_down);
    bar->addWidget(separator(left));
    bar->addWidget(m_delete);
    bar->addWidget(m_deleteAll);
    bar->addWidget(m_generate);
    bar->addWidget(m_brush);
    bar->addStretch(1);
    leftLayout->addLayout(bar);
    m_table = new QTableWidget(0, 3, left);
    m_table->setObjectName(QStringLiteral("nonlinearList"));
    m_table->setHorizontalHeaderLabels({tr("Referencia"), tr("Actúa"), tr("Escalones de carga")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setDefaultSectionSize(20);
    m_table->setColumnWidth(0, 66);
    m_table->setColumnWidth(1, 44);
    leftLayout->addWidget(m_table, 1);

    // Right: «Tipos de combinación» over «Factores de combinación».
    auto* right = new QSplitter(Qt::Vertical, splitter);
    right->setChildrenCollapsible(false);
    auto* kindsBox = new QWidget(right);
    auto* kindsLayout = new QVBoxLayout(kindsBox);
    kindsLayout->setContentsMargins(0, 0, 0, 0);
    kindsLayout->setSpacing(2);
    kindsLayout->addWidget(band(tr("Tipos de combinación"), kindsBox));
    m_kinds = new QTableWidget(kKindRowCount, 2, kindsBox);
    m_kinds->setObjectName(QStringLiteral("nonlinearKinds"));
    m_kinds->setHorizontalHeaderLabels({tr("Referencia"), tr("Usar")});
    m_kinds->verticalHeader()->hide();
    m_kinds->verticalHeader()->setDefaultSectionSize(20);
    m_kinds->horizontalHeader()->setStretchLastSection(true);
    m_kinds->setSelectionMode(QAbstractItemView::NoSelection);
    for (int r = 0; r < kKindRowCount; ++r) {
        auto* name = new QTableWidgetItem(tr(kKindRows[r]));
        name->setFlags(Qt::ItemIsEnabled);
        name->setToolTip(name->text());
        m_kinds->setItem(r, 0, name);
        auto* use = new QTableWidgetItem;
        use->setFlags(kindOfRow(r) < 0 ? Qt::ItemFlags() : (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
        use->setCheckState(Qt::Unchecked);
        m_kinds->setItem(r, 1, use);
    }
    kindsLayout->addWidget(m_kinds, 1);
    auto* factorsBox = new QWidget(right);
    auto* factorsLayout = new QVBoxLayout(factorsBox);
    factorsLayout->setContentsMargins(0, 0, 0, 0);
    factorsLayout->setSpacing(2);
    factorsLayout->addWidget(band(tr("Factores de combinación"), factorsBox));
    auto* factorBar = new QHBoxLayout;
    m_addFactor = barButton(QStringLiteral("hyp_plus"), tr("Añadir elemento a la lista"), factorsBox);
    m_copyFactor = barButton(QStringLiteral("copy"), tr("Copiar elemento seleccionado de la lista"), factorsBox);
    m_deleteFactor = barButton(QStringLiteral("hyp_cross"), tr("Borrar / Suprimir elemento seleccionado de la lista"), factorsBox);
    factorBar->addWidget(m_addFactor);
    factorBar->addWidget(m_copyFactor);
    factorBar->addWidget(m_deleteFactor);
    factorBar->addStretch(1);
    factorsLayout->addLayout(factorBar);
    m_factors = new QTableWidget(0, 2, factorsBox);
    m_factors->setObjectName(QStringLiteral("nonlinearFactors"));
    m_factors->setHorizontalHeaderLabels({tr("Hipótesis"), tr("Coeficiente")});
    m_factors->verticalHeader()->hide();
    m_factors->verticalHeader()->setDefaultSectionSize(20);
    m_factors->horizontalHeader()->setStretchLastSection(true);
    m_factors->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_factors->setSelectionMode(QAbstractItemView::SingleSelection);
    factorsLayout->addWidget(m_factors, 1);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    contentLayout()->addWidget(splitter, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    resize(346, 528);  // s_10.png

    // --- behaviour ---
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!m_refreshing && item) readListRow(item->row());
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        if (!m_refreshing) refreshPanels();
    });
    connect(m_kinds, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        const int row = currentRow();
        if (m_refreshing || !item || item->column() != 1 || row < 0) return;
        const int kind = kindOfRow(item->row());
        if (kind < 0) return;
        m_set.items[static_cast<std::size_t>(row)].kinds[static_cast<std::size_t>(kind)] = item->checkState() == Qt::Checked;
        refreshPanels();  // the two «Tensiones en el terreno» rows share one kind
    });
    connect(m_factors, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        if (!m_refreshing) readFactors();
    });
    connect(m_add, &QToolButton::clicked, this, [this] {
        NonlinearCombination c;
        c.reference = tr("Combinación %1").arg(m_set.items.size() + 1).toStdString();  // proposed reference [N]
        m_set.items.push_back(c);
        refreshList();
        m_table->selectRow(static_cast<int>(m_set.items.size()) - 1);
    });
    connect(m_copy, &QToolButton::clicked, this, [this] {
        const int row = currentRow();
        if (row < 0) return;
        m_set.items.insert(m_set.items.begin() + row + 1, m_set.items[static_cast<std::size_t>(row)]);
        refreshList();
        m_table->selectRow(row + 1);
    });
    const auto move = [this](int delta) {
        const int row = currentRow();
        const int to = row + delta;
        if (row < 0 || to < 0 || to >= static_cast<int>(m_set.items.size())) return;
        std::swap(m_set.items[static_cast<std::size_t>(row)], m_set.items[static_cast<std::size_t>(to)]);
        refreshList();
        m_table->selectRow(to);
    };
    connect(m_up, &QToolButton::clicked, this, [move] { move(-1); });
    connect(m_down, &QToolButton::clicked, this, [move] { move(+1); });
    connect(m_delete, &QToolButton::clicked, this, [this] {
        const int row = currentRow();
        if (row < 0) return;
        m_set.items.erase(m_set.items.begin() + row);
        refreshList();
        if (!m_set.items.empty()) m_table->selectRow(std::min(row, static_cast<int>(m_set.items.size()) - 1));
    });
    connect(m_deleteAll, &QToolButton::clicked, this, [this] {
        m_set.items.clear();
        refreshList();
    });
    connect(m_generate, &QToolButton::clicked, this, &NonlinearCombinationsDialog::generate);
    connect(m_addFactor, &QToolButton::clicked, this, [this] {
        const int row = currentRow();
        if (row < 0) return;
        auto& factors = m_set.items[static_cast<std::size_t>(row)].factors;
        for (const cc::core::LoadCase& lc : m_project.loadCases.cases)
            if (!factors.count(lc.id)) {
                factors[lc.id] = 1.0;  // proposed coefficient [N]
                break;
            }
        refreshPanels();
    });
    connect(m_copyFactor, &QToolButton::clicked, this, [this] {
        // Copying a row would repeat its hypothesis («Hay hipótesis repetidas en la misma combinación.», #377002): the copy takes the next
        // hypothesis without a factor, with the same coefficient.
        const int row = currentRow();
        const int f = m_factors->currentRow();
        if (row < 0 || f < 0) return;
        auto& factors = m_set.items[static_cast<std::size_t>(row)].factors;
        auto it = factors.begin();
        std::advance(it, std::min<std::ptrdiff_t>(f, static_cast<std::ptrdiff_t>(factors.size()) - 1));
        const double value = it->second;
        for (const cc::core::LoadCase& lc : m_project.loadCases.cases)
            if (!factors.count(lc.id)) {
                factors[lc.id] = value;
                break;
            }
        refreshPanels();
    });
    connect(m_deleteFactor, &QToolButton::clicked, this, [this] {
        const int row = currentRow();
        const int f = m_factors->currentRow();
        if (row < 0 || f < 0) return;
        auto& factors = m_set.items[static_cast<std::size_t>(row)].factors;
        if (f >= static_cast<int>(factors.size())) return;
        auto it = factors.begin();
        std::advance(it, f);
        factors.erase(it);
        refreshPanels();
    });
    refreshList();
    if (!m_set.items.empty()) m_table->selectRow(0);
}

int NonlinearCombinationsDialog::currentRow() const {
    const int row = m_table->currentRow();
    return row >= 0 && row < static_cast<int>(m_set.items.size()) ? row : -1;
}

void NonlinearCombinationsDialog::refreshList() {
    m_refreshing = true;
    m_table->setRowCount(static_cast<int>(m_set.items.size()));
    for (int r = 0; r < static_cast<int>(m_set.items.size()); ++r) {
        const NonlinearCombination& c = m_set.items[static_cast<std::size_t>(r)];
        m_table->setItem(r, 0, new QTableWidgetItem(qs(c.reference)));
        auto* acts = new QTableWidgetItem;
        acts->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        acts->setCheckState(c.acts ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(r, 1, acts);
        auto* steps = new QTableWidgetItem(QString::number(c.loadSteps));
        steps->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 2, steps);
    }
    const bool any = !m_set.items.empty();
    m_copy->setEnabled(any);
    m_up->setEnabled(any);
    m_down->setEnabled(any);
    m_delete->setEnabled(any);
    m_deleteAll->setEnabled(any);
    m_refreshing = false;
    refreshPanels();
}

void NonlinearCombinationsDialog::refreshPanels() {
    m_refreshing = true;
    const int row = currentRow();
    for (int r = 0; r < kKindRowCount; ++r) {
        const int kind = kindOfRow(r);
        const bool on = row >= 0 && kind >= 0 && m_set.items[static_cast<std::size_t>(row)].kinds[static_cast<std::size_t>(kind)];
        m_kinds->item(r, 1)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    m_factors->setRowCount(0);
    if (row >= 0) {
        const auto& factors = m_set.items[static_cast<std::size_t>(row)].factors;
        int f = 0;
        m_factors->setRowCount(static_cast<int>(factors.size()));
        for (const auto& [caseId, value] : factors) {
            auto* combo = new QComboBox(m_factors);
            for (const cc::core::LoadCase& lc : m_project.loadCases.cases) combo->addItem(qs(cc::core::loadCaseLabel(lc)), lc.id);
            combo->setCurrentIndex(combo->findData(caseId));
            connect(combo, &QComboBox::currentIndexChanged, this, [this](int) {
                if (!m_refreshing) readFactors();
            });
            m_factors->setCellWidget(f, 0, combo);
            auto* coefficient = new QTableWidgetItem(QString::number(value, 'g', 6));
            coefficient->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_factors->setItem(f, 1, coefficient);
            ++f;
        }
    }
    m_addFactor->setEnabled(row >= 0);
    m_copyFactor->setEnabled(row >= 0 && m_factors->rowCount() > 0);
    m_deleteFactor->setEnabled(row >= 0 && m_factors->rowCount() > 0);
    m_refreshing = false;
}

void NonlinearCombinationsDialog::readListRow(int row) {
    if (row < 0 || row >= static_cast<int>(m_set.items.size())) return;
    NonlinearCombination& c = m_set.items[static_cast<std::size_t>(row)];
    if (QTableWidgetItem* ref = m_table->item(row, 0)) c.reference = ref->text().trimmed().toStdString();
    if (QTableWidgetItem* acts = m_table->item(row, 1)) c.acts = acts->checkState() == Qt::Checked;
    if (QTableWidgetItem* steps = m_table->item(row, 2)) {
        bool ok = false;
        const int value = steps->text().trimmed().toInt(&ok);
        if (ok && value >= 1) c.loadSteps = value;
        else {
            m_refreshing = true;
            steps->setText(QString::number(c.loadSteps));  // an invalid entry is refused and the previous value comes back
            m_refreshing = false;
        }
    }
}

void NonlinearCombinationsDialog::readFactors() {
    const int row = currentRow();
    if (row < 0) return;
    // Rows can repeat a hypothesis while editing; the map keeps the last one, and Aceptar refuses repeats (#377002).
    std::map<int, double> factors;
    for (int f = 0; f < m_factors->rowCount(); ++f) {
        auto* combo = qobject_cast<QComboBox*>(m_factors->cellWidget(f, 0));
        QTableWidgetItem* item = m_factors->item(f, 1);
        if (!combo || !item) continue;
        bool ok = false;
        const double value = item->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
        if (ok && value != 0.0) factors[combo->currentData().toInt()] = value;
    }
    m_set.items[static_cast<std::size_t>(row)].factors = std::move(factors);
}

void NonlinearCombinationsDialog::generate() {
    NonlinearGenerateDialog dialog(m_set.generateKinds, this);
    if (run(dialog) != QDialog::Accepted) return;
    const cc::core::NonlinearKinds kinds = dialog.kinds();
    const cc::core::NonlinearGeneration g = cc::core::generateNonlinearCombinations(m_project, kinds);
    if (!g.error.empty()) {
        message(qs(g.error), true);
        return;
    }
    // The generated list replaces the current one [V s_10.png from an empty list; with existing combinations: N]; «Actúa» and «Escalones de
    // carga» of a combination generated again with the same reference are kept, as the update after the linear analysis does.
    std::vector<NonlinearCombination> items = g.items;
    for (NonlinearCombination& n : items)
        for (const NonlinearCombination& old : m_set.items)
            if (old.reference == n.reference) {
                n.acts = old.acts;
                n.loadSteps = old.loadSteps;
                break;
            }
    m_set.items = std::move(items);
    m_set.generateKinds = kinds;
    refreshList();
    if (!m_set.items.empty()) m_table->selectRow(0);
    message(qs(cc::core::generatedCombinationsMessage(g.items.size())), false);
}

void NonlinearCombinationsDialog::message(const QString& text, bool error) {
    messages::show(this, error ? messages::Kind::Error : messages::Kind::Information, text);
}

void NonlinearCombinationsDialog::done(int result) {
    if (result == QDialog::Accepted) {
        if (const std::string error = cc::core::validateNonlinearCombinations(m_set); !error.empty()) {
            message(qs(error), true);
            return;
        }
        // Repeated hypotheses in one combination (#377002).
        for (int f = 0; f < m_factors->rowCount(); ++f)
            for (int g = f + 1; g < m_factors->rowCount(); ++g) {
                auto* a = qobject_cast<QComboBox*>(m_factors->cellWidget(f, 0));
                auto* b = qobject_cast<QComboBox*>(m_factors->cellWidget(g, 0));
                if (a && b && a->currentData() == b->currentData()) {
                    message(tr("Hay hipótesis repetidas en la misma combinación."), true);
                    return;
                }
            }
    }
    CypeDialog::done(result);
}

}  // namespace cc::app
